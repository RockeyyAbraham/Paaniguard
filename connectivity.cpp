#include "connectivity.h"
#include "config.h"
#include "drift_compensation.h"
#include <ESP8266WiFi.h>
#include <time.h>

static ConnectivityMode currentMode = ConnectivityMode::SOFTAP;
static bool ntpSynced = false;
static const time_t SANE_EPOCH_THRESHOLD = 1700000000;
static unsigned long lastReconnectAttemptMs = 0;

void connectivity_init() {
  WiFi.mode(WIFI_STA);
  // Explicit core-level recovery: the ESP8266 core's auto-reconnect default has
  // changed across versions, and persistent flash writes on every begin() wear
  // the flash out over a multi-week deployment that retries every 30s.
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print(F("connectivity: attempting station connect to "));
  Serial.println(WIFI_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    currentMode = ConnectivityMode::STATION;
    Serial.print(F("connectivity: station connected, IP="));
    Serial.println(WiFi.localIP());
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  } else {
    currentMode = ConnectivityMode::SOFTAP;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SOFTAP_SSID, SOFTAP_PASSWORD);
    Serial.print(F("connectivity: no known network found within timeout, SoftAP fallback active. IP="));
    Serial.println(WiFi.softAPIP());
  }

  // Seed the throttle so the first background retry is a full interval away
  // rather than firing on the very first loop() pass after boot.
  lastReconnectAttemptMs = millis();
}

void connectivity_maybeReconnect() {
  // Happy path first, and unthrottled: a healthy station costs one status read.
  if (currentMode == ConnectivityMode::STATION && WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastReconnectAttemptMs < WIFI_RECONNECT_INTERVAL_MS) return;
  lastReconnectAttemptMs = now;

  if (currentMode == ConnectivityMode::STATION) {
    // Dropped association. currentMode already reports offline via
    // connectivity_isOnline(), so just re-kick the supplicant and return —
    // never wait here, the sensor/actuation path must not stall on WiFi.
    Serial.println(F("connectivity: station link lost, re-attempting association."));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    return;
  }

  // SoftAP mode. A previous background attempt may have associated in the
  // meantime, so promote before spending another begin().
  if (WiFi.status() == WL_CONNECTED) {
    currentMode = ConnectivityMode::STATION;
    Serial.print(F("connectivity: station recovered from SoftAP fallback, IP="));
    Serial.println(WiFi.localIP());
    // Same NTP kick as connectivity_init(); connectivity_maybeSyncTime() is
    // still gated on ntpSynced, which stays false if boot never synced, so a
    // late first sync still lands.
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    return;
  }

  // Retry station without tearing the AP down: the local status page is the
  // offline safety UI and must stay reachable while we probe for the router.
  Serial.println(F("connectivity: SoftAP active, retrying station in background."));
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool connectivity_isOnline() {
  return currentMode == ConnectivityMode::STATION && WiFi.status() == WL_CONNECTED;
}

ConnectivityMode connectivity_getMode() {
  return currentMode;
}

IPAddress connectivity_getIP() {
  return (currentMode == ConnectivityMode::STATION) ? WiFi.localIP() : WiFi.softAPIP();
}

void connectivity_maybeSyncTime() {
  if (ntpSynced || currentMode != ConnectivityMode::STATION) return;
  time_t now = time(nullptr);
  if (now > SANE_EPOCH_THRESHOLD) {
    drift_syncDayIndexFromEpoch(now);
    ntpSynced = true;
    Serial.println(F("connectivity: NTP time synced, drift day-index corrected."));
  }
}
