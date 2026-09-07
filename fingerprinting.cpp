// fingerprinting.cpp — rule table implementation. Sensor deviations are
// reduced to a small bitmask, then a switch statement maps known bit
// combinations to a pre-written alert. This is deliberately simple and
// fully deterministic (no ML, no statistics beyond the fixed thresholds in
// config.h) so the behavior is auditable and testable in SIMULATION_MODE.

#include "fingerprinting.h"
#include "config.h"

enum DeviationBit {
  BIT_PH_LOW    = 1 << 0,  // pH below PH_SEVERE_LOW — sharp acidic drop
  BIT_PH_HIGH   = 1 << 1,  // pH above PH_SEVERE_HIGH — sharp alkaline spike
  BIT_PH_MILD   = 1 << 2,  // pH outside safe range but not yet severe
  BIT_TDS_HIGH  = 1 << 3,  // TDS above TDS_SAFE_MAX_PPM but below severe
  BIT_TDS_SEVERE = 1 << 4  // TDS above TDS_SEVERE_MAX_PPM
};

static uint8_t computeDeviationMask(float ph, float tds) {
  uint8_t mask = 0;
  if (ph < PH_SEVERE_LOW) mask |= BIT_PH_LOW;
  else if (ph > PH_SEVERE_HIGH) mask |= BIT_PH_HIGH;
  else if (ph < PH_SAFE_MIN || ph > PH_SAFE_MAX) mask |= BIT_PH_MILD;

  if (tds > TDS_SEVERE_MAX_PPM) mask |= BIT_TDS_SEVERE;
  else if (tds > TDS_SAFE_MAX_PPM) mask |= BIT_TDS_HIGH;

  return mask;
}

FingerprintResult fingerprint_evaluate(float ph, float correctedTdsPpm) {
  FingerprintResult result;
  result.authorityContact = LOCAL_AUTHORITY_CONTACT;
  result.triggerShutoff = false;

  uint8_t mask = computeDeviationMask(ph, correctedTdsPpm);

  switch (mask) {
    case 0:
      result.severity = Severity::NONE;
      result.alertMessage = "Water quality within normal parameters.";
      break;

    case BIT_PH_MILD:
      result.severity = Severity::WATCH;
      result.alertMessage = "Mild pH deviation detected. Continuing to monitor.";
      break;

    case BIT_TDS_HIGH:
      result.severity = Severity::WARNING;
      result.alertMessage =
        "TDS rising steadily past the safe threshold. Possible mineral, "
        "salt, or gradual contamination build-up. Consider manual testing.";
      break;

    case BIT_PH_LOW:
      result.severity = Severity::WARNING;
      result.alertMessage =
        "Sharp acidic pH drop detected without a TDS shift. Possible acidic "
        "runoff. Consider manual testing before use.";
      break;

    case BIT_PH_HIGH:
      result.severity = Severity::WARNING;
      result.alertMessage =
        "Sharp alkaline pH spike detected without a TDS shift. Possible "
        "detergent or alkaline chemical runoff. Consider manual testing.";
      break;

    case BIT_TDS_SEVERE:
      result.severity = Severity::SEVERE;
      result.alertMessage =
        "Severe TDS spike detected, independent of pH. Possible heavy "
        "contamination event (e.g. industrial discharge, saltwater intrusion). "
        "Water supply automatically shut off as a precaution.";
      result.triggerShutoff = true;
      break;

    case (BIT_PH_LOW | BIT_TDS_HIGH):
    case (BIT_PH_LOW | BIT_TDS_SEVERE):
    case (BIT_PH_HIGH | BIT_TDS_HIGH):
    case (BIT_PH_HIGH | BIT_TDS_SEVERE):
      result.severity = Severity::SEVERE;
      result.alertMessage =
        "Combined pH drop/spike with elevated TDS detected. Strong signature "
        "of chemical or industrial contamination. Water supply automatically "
        "shut off as a precaution.";
      result.triggerShutoff = true;
      break;

    default:
      // Any other combination (e.g. mild pH + high TDS) — treat as a
      // warning rather than silently falling through to "normal".
      result.severity = Severity::WARNING;
      result.alertMessage =
        "Unusual combination of sensor deviations detected. Consider manual "
        "testing.";
      break;
  }

  return result;
}
