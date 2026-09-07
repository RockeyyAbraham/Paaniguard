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
void drift_recordCheckpoint(float measuredPpm);

// Mechanism 2: apply the rolling-baseline linear-trend correction (uses
// checkpoints from the last DRIFT_WINDOW_DAYS days). If fewer than 2
// checkpoints are available, returns the input unchanged.
float drift_applyBaselineCorrection(float tempCompensatedPpm);

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
