// PaaniGuard.ino — setup()/loop() only. All logic lives in the modules
// included below; this file just wires them together in order.

#include "config.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("PaaniGuard booting..."));
#if SIMULATION_MODE
  Serial.println(F("SIMULATION_MODE=1 — running on synthetic sensor data, no hardware required."));
#endif
}

void loop() {
}
