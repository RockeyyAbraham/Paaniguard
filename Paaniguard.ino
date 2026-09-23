// PaaniGuard.ino — setup()/loop() only. All logic lives in the modules
// included below; this file just wires them together in order.

#include "config.h"
#include "sensors.h"
#include "drift_compensation.h"
#include "fingerprinting.h"
#include "actuators.h"
#include "display.h"
#include "connectivity.h"
#include "webserver.h"
#include "cloud_logging.h"
#include "storage.h"

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
  storage_init();
  connectivity_init();
  webserver_init();
  cloud_logging_init();
}

void loop() {
  // Web server must stay responsive every loop iteration, independent of
  // the slower sensor-read cadence below.
  webserver_handleClient();
  // Both of these are non-blocking and self-throttling. The reconnect in
  // particular must never stall the loop: the sensor-read and actuation
  // path below is safety-critical and cannot wait on WiFi.
  connectivity_maybeReconnect();
  connectivity_maybeSyncTime();

  if (millis() - lastSensorReadMillis < SENSOR_READ_INTERVAL_MS) {
    return;
  }
  lastSensorReadMillis = millis();

  SensorReadings r;
  sensors_read(r);
  float correctedTds = drift_getCorrectedTds(r.tds_raw_ppm, r.temperatureC);
  // Sensor-health input to the rule table: once accumulated drift passes the
  // software-correctable band, a clean-looking reading is reported as
  // reduced-confidence rather than as a confident all-clear.
  bool driftExceeded = drift_needsRecalibration();
  FingerprintResult fingerprint = fingerprint_evaluate(r.ph_value, correctedTds, driftExceeded);

  // Offline-first safety response: identical whether connectivity_isOnline()
  // is true or false.
  actuators_applyFingerprint(fingerprint);
  display_render(r, correctedTds, fingerprint);

  StoredRecord rec;
  rec.timestamp = r.timestamp;
  rec.ph = r.ph_value;
  rec.correctedTdsPpm = correctedTds;
  rec.temperatureC = r.temperatureC;
  rec.flowRateLPM = r.flowRateLPM;
  rec.totalLiters = r.totalLiters;
  rec.severity = (uint8_t)fingerprint.severity;
  storage_appendRecord(rec);

  webserver_updateStatus(r, correctedTds, fingerprint);
  cloud_logging_maybePush(r, correctedTds, fingerprint);

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
