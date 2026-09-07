// simulation.cpp — synthetic sensor generator for SIMULATION_MODE. Cycles
// through four scenarios so the whole logic chain (sensors -> drift
// compensation -> fingerprinting -> actuators) can be exercised without
// real hardware:
//   1. Normal/clean water sample.
//   2. A slow simulated drift pattern across several fake "days" (backdated
//      calibration checkpoints recorded once, then held so
//      drift_compensation.cpp's baseline correction is visibly active).
//   3. Contamination pattern A — pH drop + TDS spike.
//   4. Contamination pattern B — TDS rising steadily past the severe
//      threshold, pH normal.
//
// Each scenario runs for SCENARIO_DURATION_TICKS calls to
// simulation_generate() before advancing to the next, then the cycle
// repeats. PaaniGuard.ino's normal Serial logging (pH/TDS/severity/etc,
// one line per sensor-read tick) is what shows this in action — see
// README.md for how to read that output.

#include "simulation.h"
#include "config.h"
#include "drift_compensation.h"

enum SimScenario {
  SCENARIO_CLEAN = 0,
  SCENARIO_DRIFT,
  SCENARIO_CONTAM_A,
  SCENARIO_CONTAM_B,
  SCENARIO_COUNT
};

static const unsigned long SCENARIO_DURATION_TICKS = 6;

static unsigned long simStartMillis = 0;
static unsigned long tickCount = 0;
static bool driftCheckpointsRecorded = false;

// Converts a target pH into the ADS1115 voltage that sensors.cpp's
// calibration formula (config.h PH_CALIBRATION_SLOPE/OFFSET) would convert
// back into that same pH — keeps this file's scenarios expressed in
// physical units (pH, ppm) rather than raw millivolts.
static float phToVoltageMv(float ph) {
  return ((ph - PH_CALIBRATION_OFFSET) / PH_CALIBRATION_SLOPE) * 1000.0;
}

// Inverse of sensors.cpp's cubic TDS voltage->ppm fit, solved numerically
// (the cubic doesn't invert cleanly in closed form) so scenarios can be
// expressed directly as target ppm values.
static float tdsPpmToVoltageMv(float targetPpm) {
  float loV = 0.0, hiV = 3.3;
  for (int i = 0; i < 40; i++) {
    float midV = (loV + hiV) / 2.0;
    float ppm = (TDS_CAL_COEFF_A * midV * midV * midV) - (TDS_CAL_COEFF_B * midV * midV) + (TDS_CAL_COEFF_C * midV);
    if (ppm < targetPpm) loV = midV; else hiV = midV;
  }
  return ((loV + hiV) / 2.0) * 1000.0;
}

void simulation_init() {
  simStartMillis = millis();
  tickCount = 0;
  driftCheckpointsRecorded = false;
  randomSeed(analogRead(A0));
}

static void recordBackdatedDriftCheckpoints() {
  // Simulate a probe that's been fouling for a week: readings in the
  // 342ppm reference solution have been creeping up. Backdated so
  // drift_applyBaselineCorrection() has an immediate trend to act on,
  // instead of waiting real calendar days.
  uint32_t today = drift_getCurrentDayIndex();
  drift_recordCheckpoint(345.0, (int32_t)today - 6);
  drift_recordCheckpoint(351.0, (int32_t)today - 4);
  drift_recordCheckpoint(358.0, (int32_t)today - 2);
  drift_recordCheckpoint(364.0, (int32_t)today);
  Serial.println(F("simulation: recorded 4 backdated drift checkpoints (345/351/358/364 ppm over the last 6 days) to exercise rolling-baseline correction"));
}

void simulation_generate(SimulatedRaw &out) {
  unsigned long scenarioSlot = (tickCount / SCENARIO_DURATION_TICKS) % SCENARIO_COUNT;
  tickCount++;

  float phJitterMv = random(-20, 20);
  float tdsJitterMv = random(-8, 8);
  float tempJitter = random(-20, 20) / 100.0;

  float targetPh = 7.0;
  float targetTdsPpm = 310.0;
  out.temperatureC = TDS_TEMP_REFERENCE_C + tempJitter;

  switch (scenarioSlot) {
    case SCENARIO_CLEAN:
      Serial.println(F("simulation: scenario = clean/normal water"));
      targetPh = 7.0;
      targetTdsPpm = 310.0;
      break;

    case SCENARIO_DRIFT:
      Serial.println(F("simulation: scenario = rolling-baseline drift check"));
      if (!driftCheckpointsRecorded) {
        recordBackdatedDriftCheckpoints();
        driftCheckpointsRecorded = true;
      }
      targetPh = 7.0;
      targetTdsPpm = 310.0; // same clean raw input; baseline correction should visibly pull the corrected value down
      break;

    case SCENARIO_CONTAM_A:
      Serial.println(F("simulation: scenario = contamination pattern A (pH drop + TDS spike)"));
      targetPh = 5.0;
      targetTdsPpm = 1100.0;
      break;

    case SCENARIO_CONTAM_B:
      Serial.println(F("simulation: scenario = contamination pattern B (TDS severe spike alone)"));
      targetPh = 7.0;
      targetTdsPpm = 1200.0;
      break;
  }

  out.ph_voltage_mV = phToVoltageMv(targetPh) + phJitterMv;
  out.tds_voltage_mV = tdsPpmToVoltageMv(targetTdsPpm) + tdsJitterMv;

  unsigned long elapsedSec = (millis() - simStartMillis) / 1000;
  bool flowing = (elapsedSec % 20) < 5;
  out.flowRateLPM = flowing ? (4.0 + random(-50, 50) / 100.0) : 0.0;
  out.litersThisTick = out.flowRateLPM * (SENSOR_READ_INTERVAL_MS / 60000.0);
}
