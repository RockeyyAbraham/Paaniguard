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

// Valve is fail-safe closed: PIN_RELAY_VALVE defaults LOW at boot (relay
// de-energized) and stays closed until actuators_setValve(true) is called,
// so an unexpected reset/crash never leaves water flowing unsupervised.
void actuators_setValve(bool open);
bool actuators_isValveOpen();

void actuators_setBuzzer(bool on);

void actuators_setStatusColor(uint8_t r, uint8_t g, uint8_t b);

// Applies the full offline-first safety response for a fingerprinting
// result: sets the status LED color for every severity level, and for
// Severity::SEVERE additionally shuts the valve and sounds the buzzer.
void actuators_applyFingerprint(const FingerprintResult &result);

#endif // PAANIGUARD_ACTUATORS_H
