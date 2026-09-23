// drift_compensation.h — two DISTINCT TDS correction mechanisms. Do not
// conflate them:
//
//   1. Temperature compensation (drift_applyTemperatureCompensation):
//      a standard ~2%/°C correction using the DS18B20 reading, applied to
//      EVERY reading to normalize it to a 25C reference. This corrects for
//      physics (ionic conductivity rises with temperature), not sensor
//      aging.
//
//   2. Rolling-baseline drift correction (drift_applyBaselineCorrection):
//      corrects for sensor aging/fouling over time. Tracks periodic
//      calibration checkpoints (dips into the known TDS_REFERENCE_PPM
//      reference solution) over a rolling DRIFT_WINDOW_DAYS window, fits a
//      linear trend to the drift, and applies a correction factor based on
//      days since the last calibration. Checkpoints persist to flash so the
//      trend survives a reboot.
//
// The confidence functions below (drift_getDriftPercent,
// drift_needsRecalibration) report on mechanism 2 ONLY. They describe how
// far the rolling-baseline trend has moved from the reference solution —
// they say nothing about mechanism 1, which is a per-reading physics
// correction with no accumulated state and therefore nothing to lose
// confidence in. Do not conflate them.

#ifndef PAANIGUARD_DRIFT_COMPENSATION_H
#define PAANIGUARD_DRIFT_COMPENSATION_H

#include <Arduino.h>

struct DriftCheckpoint {
  uint32_t dayIndex;   // see drift_getCurrentDayIndex()
  float measuredPpm;   // sensor's raw (temp-compensated) reading in the reference solution
};

void drift_init();

// Mechanism 1: normalize a raw TDS ppm reading to the 25C reference using
// the standard ~2%/°C rule. Call this on every single reading.
float drift_applyTemperatureCompensation(float rawTdsPpm, float temperatureC);

// Record a new calibration checkpoint — call when the probe is dipped in
// the known TDS_REFERENCE_PPM reference solution. Persists to flash.
// dayIndexOverride defaults to "today" (drift_getCurrentDayIndex()); an
// explicit value is only for backfilling historical checkpoints or for
// SIMULATION_MODE testing without waiting real calendar days.
void drift_recordCheckpoint(float measuredPpm, int32_t dayIndexOverride = -1);

// Mechanism 2: apply the rolling-baseline linear-trend correction (uses
// checkpoints from the last DRIFT_WINDOW_DAYS days). If fewer than 2
// checkpoints are available, returns the input unchanged.
float drift_applyBaselineCorrection(float tempCompensatedPpm);

// Mechanism 2 confidence: magnitude of the currently predicted baseline
// drift, as a percentage of TDS_REFERENCE_PPM. Absolute value — drift in
// either direction is an equal loss of confidence. Returns 0.0 when fewer
// than 2 checkpoints are available in the window: with insufficient data
// the honest answer is "no drift claimed", not a guess.
float drift_getDriftPercent();

// True once drift_getDriftPercent() exceeds DRIFT_MAX_CORRECTABLE_PERCENT,
// i.e. the sensor has aged past what the linear trend can still credibly
// correct for and readings should be treated as reduced-confidence until
// the probe is recalibrated.
bool drift_needsRecalibration();

// Convenience: runs both mechanisms in order (temp compensation, then
// baseline correction). This is what the rest of the firmware should call.
float drift_getCorrectedTds(float rawTdsPpm, float temperatureC);

// Current day index used to timestamp checkpoints and evaluate the rolling
// window. Backed by NTP epoch time when available (see
// drift_syncDayIndexFromEpoch, called by connectivity.cpp once online);
// falls back to a flash-persisted offline day counter otherwise. This is a
// deliberate approximation in the absence of an RTC — see drift_compensation.cpp.
uint32_t drift_getCurrentDayIndex();

// Called by connectivity.cpp once NTP time is available, to correct the
// offline day counter against real wall-clock time.
void drift_syncDayIndexFromEpoch(time_t epochSeconds);

#endif // PAANIGUARD_DRIFT_COMPENSATION_H
