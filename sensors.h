// sensors.h — raw sensor reads: pH, TDS, DS18B20 temperature, flow pulses.
// In SIMULATION_MODE these are fed by simulation.cpp instead of real
// hardware (see sensors.cpp).

#ifndef PAANIGUARD_SENSORS_H
#define PAANIGUARD_SENSORS_H

#include <Arduino.h>

struct SensorReadings {
  float ph_voltage_mV;    // raw ADS1115 voltage on the pH channel, millivolts
  float ph_value;         // converted pH, using config.h calibration constants
  float tds_voltage_mV;   // raw ADS1115 voltage on the TDS channel, millivolts
  float tds_raw_ppm;      // TDS converted to ppm, before drift/temp compensation
  float temperatureC;     // DS18B20 reading, degrees Celsius
  float flowRateLPM;      // instantaneous flow rate, liters per minute
  float totalLiters;      // cumulative liters counted since boot
  unsigned long timestamp; // millis() at time of reading
};

void sensors_init();
void sensors_read(SensorReadings &out);

// Flow pulse ISR (attached to PIN_FLOW_SENSOR in real hardware mode) and
// its pulse-count accessor. No-ops in SIMULATION_MODE.
void IRAM_ATTR sensors_flowISR();
unsigned long sensors_getFlowPulseCount();

#endif // PAANIGUARD_SENSORS_H
