// webserver.h — local web server: the status page (web/index.html, embedded
// via webpage.h), a JSON API behind it, and a captive portal so a phone that
// joins the device's own WiFi is sent straight to the page. Built for
// SoftAP-only use (a tank with no router): see connectivity_startSoftAP().

#ifndef PAANIGUARD_WEBSERVER_H
#define PAANIGUARD_WEBSERVER_H

#include <Arduino.h>
#include "sensors.h"
#include "fingerprinting.h"

// Starts the HTTP server (port 80) and the captive-portal DNS (port 53).
// Call after the WiFi access point is up.
void webserver_init();

// Call every loop() iteration (not gated by the sensor-read interval) so
// the server stays responsive.
void webserver_handleClient();

// Marks the readings as simulated: the page labels itself "Demo data".
void webserver_setDemo(bool on);

// Seconds between trend-chart samples (default 30 s, so the chart spans an
// hour). The demo build shortens it so the chart fills in a few minutes.
void webserver_setHistoryInterval(unsigned long ms);

// What the page should say about each sensor. tdsOk: the ADC answered.
// flowSeen: at least one flow pulse has ever been counted (a hall sensor
// can't be detected any other way). tempOk: a real temperature probe is
// fitted. tdsCalibrated: coefficients are bench-calibrated, not placeholders.
void webserver_setSensorStatus(bool tdsOk, bool flowSeen, bool tempOk, bool tdsCalibrated);

// Cache the latest readings/alert for the API and page. Call once per
// sensor-read cycle; also feeds the one-hour trend history.
void webserver_updateStatus(const SensorReadings &readings, float correctedTdsPpm,
                             const FingerprintResult &fingerprint, unsigned long flowPulses);

#endif // PAANIGUARD_WEBSERVER_H
