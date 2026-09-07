// webserver.h — lightweight local web server showing current readings and
// any active alert. Runs the same way in station mode or SoftAP fallback
// (see connectivity.h); it's the primary UI when the device is its own
// access point at 192.168.4.1 with no internet path to a phone/app.

#ifndef PAANIGUARD_WEBSERVER_H
#define PAANIGUARD_WEBSERVER_H

#include <Arduino.h>
#include "sensors.h"
#include "fingerprinting.h"

void webserver_init();

// Call every loop() iteration (not gated by the sensor-read interval) so
// the server stays responsive.
void webserver_handleClient();

// Cache the latest readings/alert for the root page to render. Call once
// per sensor-read cycle.
void webserver_updateStatus(const SensorReadings &readings, float correctedTdsPpm,
                             const FingerprintResult &fingerprint);

#endif // PAANIGUARD_WEBSERVER_H
