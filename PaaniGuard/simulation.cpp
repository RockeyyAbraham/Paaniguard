// simulation.cpp — synthetic sensor generator, minimal "clean water" scenario
// for this stage of the build. Drift-over-days and contamination-pattern
// scenarios are added once fingerprinting.cpp exists (see commit history /
// README) so the full alert chain can be exercised end-to-end.

#include "simulation.h"
#include "config.h"

static unsigned long simStartMillis = 0;

void simulation_init() {
  simStartMillis = millis();
  randomSeed(analogRead(A0));
}

void simulation_generate(SimulatedRaw &out) {
  // Clean water baseline: pH ~7.0, TDS ~310ppm, temp ~25C, with small
  // realistic jitter so successive readings aren't bit-identical.
  float phJitterMv = random(-30, 30);       // +/- ~0.05 pH worth of jitter
  float tdsJitterMv = random(-10, 10);      // small TDS voltage jitter
  float tempJitter = random(-20, 20) / 100.0; // +/- 0.2C

  out.ph_voltage_mV = 2515.8 + phJitterMv;
  out.tds_voltage_mV = 400.0 + tdsJitterMv;
  out.temperatureC = TDS_TEMP_REFERENCE_C + tempJitter;

  // Intermittent usage pattern: flowing for a few seconds, then idle.
  unsigned long elapsedSec = (millis() - simStartMillis) / 1000;
  bool flowing = (elapsedSec % 20) < 5;
  out.flowRateLPM = flowing ? (4.0 + random(-50, 50) / 100.0) : 0.0;
  out.litersThisTick = out.flowRateLPM * (SENSOR_READ_INTERVAL_MS / 60000.0);
}
