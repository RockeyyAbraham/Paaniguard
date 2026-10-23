#include "webserver.h"
#include "config.h"
#include "webpage.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

static ESP8266WebServer server(80);
static DNSServer dns;

static SensorReadings lastReadings = {};
static float lastCorrectedTdsPpm = 0.0;
static unsigned long lastFlowPulses = 0;
static FingerprintResult lastFingerprint = {
  Severity::NONE, "No readings yet.", LOCAL_AUTHORITY_CONTACT, false
};
static bool tdsOk = false, flowSeen = false, tempOk = false, tdsCalibrated = false;

// One hour of TDS history at one sample every 30 s. 0xFFFF marks "no data"
// (probe missing at that moment, or the device had not been running yet).
static const uint16_t HIST_NONE = 0xFFFF;
static const uint8_t  HIST_LEN = 120;
static unsigned long histIntervalMs = 30000;   // see webserver_setHistoryInterval()
static bool demoData = false;
static uint16_t hist[HIST_LEN];
static uint8_t histHead = 0;      // next slot to write
static unsigned long lastHistMs = 0;
static bool histStarted = false;

static const IPAddress PORTAL_IP(192, 168, 4, 1);

// Escapes the two characters that would break a JSON string. The messages
// served here are fixed firmware strings, so nothing else needs handling.
static size_t jsonStr(char *dst, size_t cap, const char *s) {
  size_t n = 0;
  if (cap < 3) return 0;
  dst[n++] = '"';
  for (; *s && n + 3 < cap; ++s) {
    if (*s == '"' || *s == '\\') dst[n++] = '\\';
    dst[n++] = *s;
  }
  dst[n++] = '"';
  dst[n] = '\0';
  return n;
}

static void handleRoot() {
  server.sendHeader(F("Cache-Control"), F("no-cache"));
  server.send_P(200, "text/html; charset=utf-8", WEBPAGE_HTML);
}

static void handleStatus() {
  char msg[300], contact[160];
  jsonStr(msg, sizeof msg, lastFingerprint.alertMessage);
  jsonStr(contact, sizeof contact, lastFingerprint.authorityContact);

  char buf[900];
  snprintf(buf, sizeof buf,
    "{\"uptime\":%lu,\"demo\":%s,"
    "\"verdict\":{\"level\":%d,\"message\":%s},"
    "\"tds\":{\"ppm\":%.1f,\"mv\":%.0f,\"ok\":%s,\"calibrated\":%s,\"safeMax\":%d,\"severeMax\":%d},"
    "\"flow\":{\"lpm\":%.2f,\"total\":%.2f,\"pulses\":%lu,\"ok\":true,\"seen\":%s},"
    "\"temp\":{\"c\":%.1f,\"ok\":%s},\"ph\":{\"ok\":false},"
    "\"net\":{\"ssid\":\"%s\",\"ip\":\"%s\",\"clients\":%d},"
    "\"sys\":{\"heap\":%u},"
    "\"contact\":%s}",
    millis() / 1000UL, demoData ? "true" : "false",
    (int)lastFingerprint.severity, msg,
    lastCorrectedTdsPpm, lastReadings.tds_voltage_mV, tdsOk ? "true" : "false",
    tdsCalibrated ? "true" : "false", (int)TDS_SAFE_MAX_PPM, (int)TDS_SEVERE_MAX_PPM,
    lastReadings.flowRateLPM, lastReadings.totalLiters, lastFlowPulses, flowSeen ? "true" : "false",
    lastReadings.temperatureC, tempOk ? "true" : "false",
    SOFTAP_SSID, WiFi.softAPIP().toString().c_str(), (int)WiFi.softAPgetStationNum(),
    (unsigned)ESP.getFreeHeap(),
    contact);
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.send(200, "application/json", buf);
}

static void handleHistory() {
  char buf[HIST_LEN * 6 + 48];
  size_t n = snprintf(buf, sizeof buf, "{\"interval\":%lu,\"ppm\":[", histIntervalMs / 1000UL);
  for (uint8_t i = 0; i < HIST_LEN; ++i) {
    uint16_t v = hist[(histHead + i) % HIST_LEN];   // oldest first
    if (i) buf[n++] = ',';
    if (v == HIST_NONE) n += snprintf(buf + n, sizeof buf - n, "null");
    else                n += snprintf(buf + n, sizeof buf - n, "%u", v);
  }
  snprintf(buf + n, sizeof buf - n, "]}");
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.send(200, "application/json", buf);
}

// Captive portal: phones probe a known URL (/generate_204 on Android,
// /hotspot-detect.html on Apple, /connecttest.txt on Windows, ...) to decide
// whether the network has internet. Any unknown URL is redirected to the page,
// so the probe fails the "is this real internet" test and the phone pops the
// sign-in sheet pointing at the status page.
static void handleRedirect() {
  server.sendHeader(F("Location"), String(F("http://")) + PORTAL_IP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

void webserver_init() {
  for (uint8_t i = 0; i < HIST_LEN; ++i) hist[i] = HIST_NONE;

  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", PORTAL_IP);

  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/history", handleHistory);
  server.onNotFound(handleRedirect);
  server.begin();
  Serial.println(F("webserver: status page + API + captive portal on 192.168.4.1"));
}

void webserver_handleClient() {
  dns.processNextRequest();
  server.handleClient();
}

void webserver_setDemo(bool on) {
  demoData = on;
}

void webserver_setHistoryInterval(unsigned long ms) {
  histIntervalMs = ms < 1000 ? 1000 : ms;
}

void webserver_setSensorStatus(bool tdsOkNow, bool flowSeenNow, bool tempOkNow, bool tdsCalibratedNow) {
  tdsOk = tdsOkNow;
  flowSeen = flowSeenNow;
  tempOk = tempOkNow;
  tdsCalibrated = tdsCalibratedNow;
}

void webserver_updateStatus(const SensorReadings &readings, float correctedTdsPpm,
                             const FingerprintResult &fingerprint, unsigned long flowPulses) {
  lastReadings = readings;
  lastCorrectedTdsPpm = correctedTdsPpm;
  lastFingerprint = fingerprint;
  lastFlowPulses = flowPulses;

  if (!histStarted || millis() - lastHistMs >= histIntervalMs) {
    histStarted = true;
    lastHistMs = millis();
    hist[histHead] = tdsOk ? (uint16_t)constrain((long)correctedTdsPpm, 0L, 60000L) : HIST_NONE;
    histHead = (histHead + 1) % HIST_LEN;
  }
}
