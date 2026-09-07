// PaaniGuard.ino — setup()/loop() only. All logic lives in the modules
// included below; this file just wires them together in order.

#include "config.h"
#include "sensors.h"
#include "drift_compensation.h"
#include "fingerprinting.h"
#include "actuators.h"
#include "display.h"

static unsigned long lastSensorReadMillis = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("PaaniGuard booting..."));
#if SIMULATION_MODE
  Serial.println(F("SIMULATION_MODE=1 — running on synthetic sensor data, no hardware required."));
#endif

  sensors_init();
  drift_init();
  actuators_init();
  display_init();
}

void loop() {
  if (millis() - lastSensorReadMillis < SENSOR_READ_INTERVAL_MS) {
    return;
  }
  lastSensorReadMillis = millis();

  SensorReadings r;
  sensors_read(r);
  float correctedTds = drift_getCorrectedTds(r.tds_raw_ppm, r.temperatureC);
  FingerprintResult fingerprint = fingerprint_evaluate(r.ph_value, correctedTds);
  actuators_applyFingerprint(fingerprint);
  display_render(r, correctedTds, fingerprint);

  Serial.print(F("pH="));
  Serial.print(r.ph_value);
  Serial.print(F(" rawTDS="));
  Serial.print(r.tds_raw_ppm);
  Serial.print(F(" correctedTDS="));
  Serial.print(correctedTds);
  Serial.print(F(" tempC="));
  Serial.print(r.temperatureC);
  Serial.print(F(" flowLPM="));
  Serial.print(r.flowRateLPM);
  Serial.print(F(" totalL="));
  Serial.print(r.totalLiters);
  Serial.print(F(" severity="));
  Serial.print((int)fingerprint.severity);
  Serial.print(F(" shutoff="));
  Serial.println(fingerprint.triggerShutoff ? "YES" : "no");
}
