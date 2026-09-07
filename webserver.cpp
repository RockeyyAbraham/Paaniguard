#include "webserver.h"
#include "config.h"
#include <ESP8266WebServer.h>

static ESP8266WebServer server(80);

static SensorReadings lastReadings = {};
static float lastCorrectedTdsPpm = 0.0;
static FingerprintResult lastFingerprint = {
  Severity::NONE, "No readings yet.", LOCAL_AUTHORITY_CONTACT, false
};

static const char *severityLabel(Severity s) {
  switch (s) {
    case Severity::NONE:    return "OK";
    case Severity::WATCH:   return "WATCH";
    case Severity::WARNING: return "WARNING";
    case Severity::SEVERE:  return "ALERT";
  }
  return "?";
}

static void handleRoot() {
  String html;
  html.reserve(1024);
  html += F("<!DOCTYPE html><html><head><title>PaaniGuard</title>"
            "<meta http-equiv=\"refresh\" content=\"5\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "</head><body style=\"font-family:sans-serif\">");
  html += F("<h1>PaaniGuard status</h1>");
  html += F("<p>pH: ");
  html += String(lastReadings.ph_value, 2);
  html += F("</p><p>TDS (corrected): ");
  html += String(lastCorrectedTdsPpm, 0);
  html += F(" ppm</p><p>Temperature: ");
  html += String(lastReadings.temperatureC, 1);
  html += F(" C</p><p>Flow rate: ");
  html += String(lastReadings.flowRateLPM, 1);
  html += F(" L/min</p><p>Total usage: ");
  html += String(lastReadings.totalLiters, 1);
  html += F(" L</p><h2>Status: ");
  html += severityLabel(lastFingerprint.severity);
  html += F("</h2><p>");
  html += lastFingerprint.alertMessage;
  html += F("</p>");
  if (lastFingerprint.severity == Severity::SEVERE) {
    html += F("<p><b>");
    html += lastFingerprint.authorityContact;
    html += F("</b></p>");
  }
  html += F("</body></html>");
  server.send(200, "text/html", html);
}

void webserver_init() {
  server.on("/", handleRoot);
  server.begin();
  Serial.println(F("webserver: local status page started on port 80"));
}

void webserver_handleClient() {
  server.handleClient();
}

void webserver_updateStatus(const SensorReadings &readings, float correctedTdsPpm,
                             const FingerprintResult &fingerprint) {
  lastReadings = readings;
  lastCorrectedTdsPpm = correctedTdsPpm;
  lastFingerprint = fingerprint;
}
