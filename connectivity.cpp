#include "connectivity.h"
#include "config.h"
#include "drift_compensation.h"
#include <ESP8266WiFi.h>
#include <time.h>

static ConnectivityMode currentMode = ConnectivityMode::SOFTAP;
static bool ntpSynced = false;
static const time_t SANE_EPOCH_THRESHOLD = 1700000000;

void connectivity_init() {
  WiFi.mode(WIFI_STA);
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
