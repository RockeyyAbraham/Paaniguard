// simulation.h — synthetic sensor data generator used when SIMULATION_MODE
// is enabled (see config.h). Lets the whole firmware logic chain (drift
// compensation, fingerprinting, actuator response) be exercised and
// verified over Serial with no physical sensors attached.
//
// NOTE: this header is intentionally minimal in this commit — it grows a
// scenario cycler (clean water / drift-over-days / contamination patterns)
// once fingerprinting.cpp exists to drive alerts against. See simulation.cpp.

#ifndef PAANIGUARD_SIMULATION_H
#define PAANIGUARD_SIMULATION_H

#include <Arduino.h>

struct SimulatedRaw {
  float ph_voltage_mV;   // fake ADS1115 pH-channel voltage
  float tds_voltage_mV;  // fake ADS1115 TDS-channel voltage
  float temperatureC;    // fake DS18B20 reading
  float flowRateLPM;     // fake instantaneous flow rate
  float litersThisTick;  // fake liters accumulated since the last call
};

void simulation_init();
void simulation_generate(SimulatedRaw &out);

#endif // PAANIGUARD_SIMULATION_H
