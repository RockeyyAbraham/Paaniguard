// actuators.h — relay/solenoid, buzzer, and WS2812B status LED control.
// Deliberately dumb and synchronous: no cloud dependency, no async state
// machine. fingerprinting.cpp's severity decides what happens here, and it
// happens the same way whether WiFi is connected or not (offline-first
// safety response).

#ifndef PAANIGUARD_ACTUATORS_H
#define PAANIGUARD_ACTUATORS_H

#include <Arduino.h>
#include "fingerprinting.h"

void actuators_init();

// Valve is fail-safe closed: PIN_RELAY_VALVE is driven to
// RELAY_LEVEL_VALVE_CLOSED at boot (relay de-energized) and only opens once
// actuators_applyFingerprint() sees an acceptable reading, so an unexpected
// reset/crash never leaves water flowing unsupervised. A shutoff LATCHES:
// once triggered the valve stays closed until VALVE_REOPEN_CLEAN_READINGS
// consecutive clean readings arrive, and the latch itself is never persisted
// so a power cycle lands closed-and-unlatched rather than stuck.
void actuators_setValve(bool open);
bool actuators_isValveOpen();

// True while the valve is being held closed by a contamination latch, so the
// webserver/display can distinguish "shut because of bad water" from
// "not open yet".
bool actuators_isShutoffLatched();

void actuators_setBuzzer(bool on);

void actuators_setStatusColor(uint8_t r, uint8_t g, uint8_t b);

// Applies the full offline-first safety response for a fingerprinting
// result: sets the status LED color for every severity level, sounds the
// buzzer on Severity::SEVERE, opens the valve while readings are acceptable,
// and latches it closed on any result with triggerShutoff set.
void actuators_applyFingerprint(const FingerprintResult &result);

#endif // PAANIGUARD_ACTUATORS_H
