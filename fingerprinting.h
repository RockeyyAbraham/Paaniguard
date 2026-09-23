// fingerprinting.h — rule-based contamination fingerprinting. This is a
// plain switch/lookup table over sensor-deviation combinations, NOT machine
// learning. Each matched pattern maps to a pre-written plain-language alert
// string plus a local-authority contact placeholder. This module only
// produces output (a result struct); it never dials or texts anyone.

#ifndef PAANIGUARD_FINGERPRINTING_H
#define PAANIGUARD_FINGERPRINTING_H

#include <Arduino.h>

enum class Severity {
  NONE,     // within normal parameters
  WATCH,    // minor deviation, worth logging, no action
  WARNING,  // notable deviation, surface prominently, no shutoff
  SEVERE    // matches a known contamination fingerprint — trigger safety response
};

struct FingerprintResult {
  Severity severity;
  const char *alertMessage;      // plain-language description of what was detected
  const char *authorityContact;  // placeholder local-authority contact text (display only)
  bool triggerShutoff;           // true only for SEVERE matches
};

// Evaluates the current (already drift/temp-corrected) readings against the
// rule table and returns the matched fingerprint. Pure function — has no
// side effects, does not touch actuators itself (see actuators.h for that).
// driftExceeded reports that the TDS sensor has drifted beyond the software-
// correctable range; it only ever adds a reduced-confidence advisory when no
// real deviation is present, and never downgrades a contamination result.
FingerprintResult fingerprint_evaluate(float ph, float correctedTdsPpm, bool driftExceeded = false);

#endif // PAANIGUARD_FINGERPRINTING_H
