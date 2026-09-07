// cloud_logging.h — pushes readings to ThingSpeak when WiFi/internet is
// actually available; skips silently and lets storage.cpp keep buffering
// locally otherwise. ThingSpeak was chosen over Blynk for this project:
// a plain REST field-write needs no persistent connection or companion
// app and comfortably fits a single-channel hobby/bench deployment.

#ifndef PAANIGUARD_CLOUD_LOGGING_H
#define PAANIGUARD_CLOUD_LOGGING_H

#include <Arduino.h>
#include "sensors.h"
#include "fingerprinting.h"

void cloud_logging_init();

// Pushes to ThingSpeak if connectivity_isOnline() and
// THINGSPEAK_PUSH_INTERVAL_MS has elapsed since the last push; otherwise
// returns immediately without touching the network.
void cloud_logging_maybePush(const SensorReadings &readings, float correctedTdsPpm,
                              const FingerprintResult &fingerprint);

#endif // PAANIGUARD_CLOUD_LOGGING_H
