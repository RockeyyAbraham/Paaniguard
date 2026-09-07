// drift_compensation.cpp — see drift_compensation.h for the two-mechanism
// design. Checkpoints persist to LittleFS at STORAGE_CHECKPOINT_PATH so the
// rolling-baseline trend survives a reboot.

#include "drift_compensation.h"
#include "config.h"
#include <LittleFS.h>
#include <time.h>

struct DriftPersistedState {
  uint32_t magic;                              // format guard
  uint32_t bootDayOffset;                      // see drift_getCurrentDayIndex()
  uint8_t checkpointCount;
  DriftCheckpoint checkpoints[DRIFT_MAX_CHECKPOINTS];
};

static const uint32_t DRIFT_STATE_MAGIC = 0x50474454; // "PGDT"

static DriftPersistedState state;
static unsigned long dayIndexAnchorMillis = 0;

// NTP epoch times below this are treated as "clock not synced yet"
// (roughly 2023-11-14). Without an RTC, this is the practical way to tell
// "we have real wall-clock time" apart from "board just booted, no NTP yet".
static const time_t SANE_EPOCH_THRESHOLD = 1700000000;

static void loadState() {
  memset(&state, 0, sizeof(state));
  if (!LittleFS.exists(STORAGE_CHECKPOINT_PATH)) {
    state.magic = DRIFT_STATE_MAGIC;
    return;
  }
  File f = LittleFS.open(STORAGE_CHECKPOINT_PATH, "r");
  if (!f) {
    state.magic = DRIFT_STATE_MAGIC;
    return;
  }
  size_t read = f.read((uint8_t *)&state, sizeof(state));
  f.close();
  if (read != sizeof(state) || state.magic != DRIFT_STATE_MAGIC) {
    // Corrupt or first-ever boot — start clean rather than trust garbage.
    memset(&state, 0, sizeof(state));
    state.magic = DRIFT_STATE_MAGIC;
  }
}

static void saveState() {
  File f = LittleFS.open(STORAGE_CHECKPOINT_PATH, "w");
  if (!f) {
    Serial.println(F("drift_compensation: failed to open checkpoint file for write"));
    return;
  }
  f.write((const uint8_t *)&state, sizeof(state));
  f.close();
}

void drift_init() {
  LittleFS.begin();
  loadState();
  dayIndexAnchorMillis = millis();
}

float drift_applyTemperatureCompensation(float rawTdsPpm, float temperatureC) {
  // Standard ~2%/°C rule: raw readings taken above 25C read artificially
  // high (ionic conductivity increases with temperature), so we divide the
  // raw value back down to what it would read at the 25C reference.
  float compensationFactor = 1.0 + TDS_TEMP_COMPENSATION_COEFF * (temperatureC - TDS_TEMP_REFERENCE_C);
  if (compensationFactor <= 0.0) {
    // Guard against absurd/garbage temperature readings driving this negative.
    return rawTdsPpm;
  }
  return rawTdsPpm / compensationFactor;
}

uint32_t drift_getCurrentDayIndex() {
  time_t now = time(nullptr);
  if (now > SANE_EPOCH_THRESHOLD) {
    return (uint32_t)(now / 86400L);
  }
  // Offline fallback: no RTC, so we approximate "days since last known
  // wall-clock sync" as bootDayOffset (persisted) plus uptime-since-boot.
  // This treats any power-off downtime as instantaneous, which is a known
  // limitation — connectivity.cpp corrects this via
  // drift_syncDayIndexFromEpoch() as soon as NTP time is available.
  return state.bootDayOffset + (uint32_t)((millis() - dayIndexAnchorMillis) / 86400000UL);
}

void drift_syncDayIndexFromEpoch(time_t epochSeconds) {
  if (epochSeconds <= SANE_EPOCH_THRESHOLD) {
    return;
  }
  uint32_t realDay = (uint32_t)(epochSeconds / 86400L);
  state.bootDayOffset = realDay;
  dayIndexAnchorMillis = millis();
  saveState();
}

void drift_recordCheckpoint(float measuredPpm) {
  DriftCheckpoint cp;
  cp.dayIndex = drift_getCurrentDayIndex();
  cp.measuredPpm = measuredPpm;

  if (state.checkpointCount < DRIFT_MAX_CHECKPOINTS) {
    state.checkpoints[state.checkpointCount++] = cp;
  } else {
    // Ring-buffer: drop the oldest checkpoint to make room.
    for (uint8_t i = 1; i < DRIFT_MAX_CHECKPOINTS; i++) {
      state.checkpoints[i - 1] = state.checkpoints[i];
    }
    state.checkpoints[DRIFT_MAX_CHECKPOINTS - 1] = cp;
  }
  saveState();

  Serial.print(F("drift_compensation: recorded checkpoint day="));
  Serial.print(cp.dayIndex);
  Serial.print(F(" measuredPpm="));
  Serial.println(cp.measuredPpm);
}

float drift_applyBaselineCorrection(float tempCompensatedPpm) {
  uint32_t currentDay = drift_getCurrentDayIndex();
  uint32_t windowStart = (currentDay > DRIFT_WINDOW_DAYS) ? (currentDay - DRIFT_WINDOW_DAYS) : 0;

  // Least-squares linear fit of drift (measured - reference) vs. dayIndex,
  // using only checkpoints inside the rolling window.
  double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
  int n = 0;
  for (uint8_t i = 0; i < state.checkpointCount; i++) {
    if (state.checkpoints[i].dayIndex < windowStart) continue;
    double x = state.checkpoints[i].dayIndex;
    double y = state.checkpoints[i].measuredPpm - TDS_REFERENCE_PPM;
    sumX += x;
    sumY += y;
    sumXY += x * y;
    sumXX += x * x;
    n++;
  }

  if (n < 2) {
    // Not enough calibration history yet to fit a trend — pass through.
    return tempCompensatedPpm;
  }

  double denom = (n * sumXX) - (sumX * sumX);
  if (fabs(denom) < 1e-9) {
    // All checkpoints on the same day — can't fit a slope, pass through.
    return tempCompensatedPpm;
  }

  double slope = ((n * sumXY) - (sumX * sumY)) / denom;
  double intercept = (sumY - slope * sumX) / n;
  double predictedDrift = slope * currentDay + intercept;

  return tempCompensatedPpm - (float)predictedDrift;
}

float drift_getCorrectedTds(float rawTdsPpm, float temperatureC) {
  float tempCompensated = drift_applyTemperatureCompensation(rawTdsPpm, temperatureC);
  return drift_applyBaselineCorrection(tempCompensated);
}
