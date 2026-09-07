#include "cloud_logging.h"
#include "config.h"
#include "connectivity.h"
#include <ESP8266WiFi.h>
#include <ThingSpeak.h>

static WiFiClient client;
static unsigned long lastPushMillis = 0;

void cloud_logging_init() {
  ThingSpeak.begin(client);
}

void cloud_logging_maybePush(const SensorReadings &readings, float correctedTdsPpm,
                              const FingerprintResult &fingerprint) {
  if (!connectivity_isOnline()) {
    return; // skip silently — storage.cpp keeps buffering locally regardless
  }
  if (millis() - lastPushMillis < THINGSPEAK_PUSH_INTERVAL_MS) {
    return;
  }
  lastPushMillis = millis();

  ThingSpeak.setField(1, readings.ph_value);
  ThingSpeak.setField(2, correctedTdsPpm);
  ThingSpeak.setField(3, readings.temperatureC);
  ThingSpeak.setField(4, readings.flowRateLPM);
  ThingSpeak.setField(5, readings.totalLiters);
  ThingSpeak.setField(6, (float)(int)fingerprint.severity);

  int httpCode = ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_WRITE_API_KEY);
  if (httpCode == 200) {
    Serial.println(F("cloud_logging: pushed reading to ThingSpeak"));
  } else {
    Serial.print(F("cloud_logging: ThingSpeak push failed, HTTP code "));
    Serial.println(httpCode);
  }
}
