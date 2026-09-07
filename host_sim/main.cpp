// main.cpp — host-side test harness. There is no ESP8266 board attached to
// this project yet, so this compiles the REAL drift_compensation.cpp and
// fingerprinting.cpp (unmodified logic, same files the firmware ships)
// against the Arduino.h/LittleFS.h shims in this directory, and drives them
// through the same scenarios simulation.cpp cycles through on-device:
// clean water, rolling-baseline drift, and two contamination fingerprints.
// This is what stands in for a live Serial capture until real hardware is
// available — see README.md.

#include "../config.h"
#include "../drift_compensation.h"
#include "../fingerprinting.h"
#include <cstdio>

static const char *severityName(Severity s) {
  switch (s) {
    case Severity::NONE:    return "NONE";
    case Severity::WATCH:   return "WATCH";
    case Severity::WARNING: return "WARNING";
    case Severity::SEVERE:  return "SEVERE";
  }
  return "?";
}

static void runReading(const char *label, float rawTdsPpm, float tempC, float ph) {
  float tempCompensated = drift_applyTemperatureCompensation(rawTdsPpm, tempC);
  float baselineCorrected = drift_applyBaselineCorrection(tempCompensated);
  FingerprintResult fp = fingerprint_evaluate(ph, baselineCorrected);

  std::printf("[%-16s] pH=%5.2f  rawTDS=%7.1f  tempC=%4.1f  ->  tempCompTDS=%7.1f  ->  baselineCorrectedTDS=%7.1f\n",
              label, ph, rawTdsPpm, tempC, tempCompensated, baselineCorrected);
  std::printf("%-20s severity=%-8s shutoff=%-3s alert=\"%s\"\n",
              "", severityName(fp.severity), fp.triggerShutoff ? "YES" : "no", fp.alertMessage);
}

int main() {
  drift_init();

  std::printf("=== Scenario 1: normal/clean water sample ===\n");
  runReading("clean@25C", 310.0f, 25.0f, 7.0f);
  runReading("clean@30C", 310.0f, 30.0f, 7.0f); // isolates temperature compensation: same raw TDS, warmer water

  std::printf("\n=== Scenario 2: rolling-baseline drift correction ===\n");
  uint32_t today = drift_getCurrentDayIndex();
  std::printf("recording 4 backdated calibration checkpoints (probe dipped in %.0fppm reference), simulating a week of fouling drift:\n", (float)TDS_REFERENCE_PPM);
  drift_recordCheckpoint(345.0f, (int32_t)today - 6);
  drift_recordCheckpoint(351.0f, (int32_t)today - 4);
  drift_recordCheckpoint(358.0f, (int32_t)today - 2);
  drift_recordCheckpoint(364.0f, (int32_t)today);
  runReading("post-drift-fit", 310.0f, 25.0f, 7.0f); // same raw input as scenario 1 — baseline correction should now pull it down

  std::printf("\n=== Scenario 3: contamination pattern A (pH drop + TDS spike) ===\n");
  runReading("contam-A", 1100.0f, 25.0f, 5.0f);

  std::printf("\n=== Scenario 4: contamination pattern B (TDS severe spike alone, pH normal) ===\n");
  runReading("contam-B", 1200.0f, 25.0f, 7.0f);

  return 0;
}
