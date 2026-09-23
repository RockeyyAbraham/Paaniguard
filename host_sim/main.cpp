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
  // Mirror Paaniguard.ino exactly: the same sensor-health flag the firmware
  // feeds the rule table is fed here, so the harness cannot drift away from
  // on-device behavior.
  bool driftExceeded = drift_needsRecalibration();
  FingerprintResult fp = fingerprint_evaluate(ph, baselineCorrected, driftExceeded);

  std::printf("[%-16s] pH=%5.2f  rawTDS=%7.1f  tempC=%4.1f  ->  tempCompTDS=%7.1f  ->  baselineCorrectedTDS=%7.1f\n",
              label, ph, rawTdsPpm, tempC, tempCompensated, baselineCorrected);
  std::printf("%-20s drift=%5.1f%%  recalNeeded=%-3s\n",
              "", drift_getDriftPercent(), driftExceeded ? "YES" : "no");
  std::printf("%-20s severity=%-8s shutoff=%-3s alert=\"%s\"\n",
              "", severityName(fp.severity), fp.triggerShutoff ? "YES" : "no", fp.alertMessage);
}

// Minimal assertion helper — the harness has no test framework and does not
// need one; it needs a visible PASS/FAIL per claim the report will cite.
static int failures = 0;
static void check(const char *claim, bool condition) {
  std::printf("    %-4s %s\n", condition ? "PASS" : "FAIL", claim);
  if (!condition) failures++;
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

  // -------------------------------------------------------------------------
  // Scenario 5 exercises the project's core contribution: the base paper
  // measured low-cost TDS drift and recommended software correction, but left
  // it unimplemented. Here the sensor has drifted past the band the software
  // can still correct for, and the device says so instead of reporting a
  // confident all-clear it can no longer justify.
  // -------------------------------------------------------------------------
  std::printf("\n=== Scenario 5: drift beyond the software-correctable band ===\n");
  std::printf("recording steeper checkpoints (stacked on scenario 2's) to push accumulated drift past %.0f%%:\n",
              (float)DRIFT_MAX_CORRECTABLE_PERCENT);
  drift_recordCheckpoint(430.0f, (int32_t)today - 3);
  drift_recordCheckpoint(455.0f, (int32_t)today - 2);
  drift_recordCheckpoint(480.0f, (int32_t)today - 1);
  drift_recordCheckpoint(505.0f, (int32_t)today);

  check("drift_getDriftPercent() now exceeds the correctable band",
        drift_getDriftPercent() > (float)DRIFT_MAX_CORRECTABLE_PERCENT);
  check("drift_needsRecalibration() is true", drift_needsRecalibration());

  runReading("drift-advisory", 310.0f, 25.0f, 7.0f);
  {
    // Clean pH and TDS, but the sensor is no longer trustworthy: advisory only.
    FingerprintResult fp = fingerprint_evaluate(7.0f, 310.0f, true);
    check("clean readings + drift flag -> WATCH", fp.severity == Severity::WATCH);
    check("clean readings + drift flag -> no shutoff", fp.triggerShutoff == false);
  }

  // -------------------------------------------------------------------------
  // Scenario 6 is the precedence guarantee. A degraded sensor must never be
  // allowed to soften a real contamination alert — if it could, a drifting
  // probe would become a way to silence the safety response.
  // -------------------------------------------------------------------------
  std::printf("\n=== Scenario 6: contamination outranks the drift advisory ===\n");
  {
    FingerprintResult fp = fingerprint_evaluate(5.0f, 1100.0f, true);
    check("contaminated + drift flag -> still SEVERE", fp.severity == Severity::SEVERE);
    check("contaminated + drift flag -> shutoff still asserted", fp.triggerShutoff == true);

    FingerprintResult without = fingerprint_evaluate(5.0f, 1100.0f, false);
    check("drift flag does not alter the contamination verdict",
          fp.severity == without.severity && fp.triggerShutoff == without.triggerShutoff);
  }

  std::printf("\n=== %s: %d check(s) failed ===\n", failures == 0 ? "ALL CHECKS PASSED" : "FAILURES", failures);
  return failures == 0 ? 0 : 1;
}
