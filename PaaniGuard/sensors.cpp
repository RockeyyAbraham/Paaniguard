// sensors.cpp — raw sensor reads. In SIMULATION_MODE, real hardware calls
// are replaced by simulation.cpp's synthetic generator (see config.h).

#include "sensors.h"
#include "config.h"

#if SIMULATION_MODE
#include "simulation.h"
#else
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>

static Adafruit_ADS1115 ads;
static OneWire oneWire(PIN_ONEWIRE_TEMP);
static DallasTemperature dallas(&oneWire);
#endif

static volatile unsigned long flowPulseCount = 0;
static unsigned long lastFlowCalcMillis = 0;
static unsigned long lastFlowPulseSnapshot = 0;
static float totalLitersAccum = 0.0;
static float lastFlowRateLPM = 0.0;

void IRAM_ATTR sensors_flowISR() {
  flowPulseCount++;
}

unsigned long sensors_getFlowPulseCount() {
  noInterrupts();
  unsigned long count = flowPulseCount;
  interrupts();
  return count;
}

void sensors_init() {
#if SIMULATION_MODE
  simulation_init();
#else
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  ads.begin(ADS1115_I2C_ADDRESS);
  dallas.begin();
  pinMode(PIN_FLOW_SENSOR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_SENSOR), sensors_flowISR, FALLING);
#endif
  lastFlowCalcMillis = millis();
}

// Converts accumulated ISR pulse counts (since the last call) into an
// instantaneous flow rate and adds to the running liters total. Returns
// the previous rate unchanged if called again too soon to be meaningful.
static float computeFlowRateLPM() {
  unsigned long now = millis();
  unsigned long elapsedMs = now - lastFlowCalcMillis;
  if (elapsedMs < 1000) {
    return lastFlowRateLPM;
  }
  unsigned long count = sensors_getFlowPulseCount();
  unsigned long deltaPulses = count - lastFlowPulseSnapshot;
  lastFlowPulseSnapshot = count;
  lastFlowCalcMillis = now;

  float liters = deltaPulses / FLOW_PULSES_PER_LITER;
  totalLitersAccum += liters;
  float minutes = elapsedMs / 60000.0;
  lastFlowRateLPM = liters / minutes;
  return lastFlowRateLPM;
}

void sensors_read(SensorReadings &out) {
  out.timestamp = millis();

#if SIMULATION_MODE
  SimulatedRaw sim;
  simulation_generate(sim);
  out.ph_voltage_mV = sim.ph_voltage_mV;
  out.tds_voltage_mV = sim.tds_voltage_mV;
  out.temperatureC = sim.temperatureC;
  out.flowRateLPM = sim.flowRateLPM;
  totalLitersAccum += sim.litersThisTick;
#else
  int16_t phRaw = ads.readADC_SingleEnded(ADS1115_CHANNEL_PH);
  out.ph_voltage_mV = ads.computeVolts(phRaw) * 1000.0;

  int16_t tdsRaw = ads.readADC_SingleEnded(ADS1115_CHANNEL_TDS);
  out.tds_voltage_mV = ads.computeVolts(tdsRaw) * 1000.0;

  dallas.requestTemperatures();
  out.temperatureC = dallas.getTempCByIndex(0);

  out.flowRateLPM = computeFlowRateLPM();
#endif

  // pH: linear fit between the two buffer-solution calibration points
  // (see config.h — PLACEHOLDER until bench-calibrated with pH 4.0/7.0).
  out.ph_value = PH_CALIBRATION_SLOPE * (out.ph_voltage_mV / 1000.0) + PH_CALIBRATION_OFFSET;

  // TDS: cubic voltage->ppm fit (see config.h — PLACEHOLDER until
  // bench-calibrated against the 342ppm reference solution).
  float v = out.tds_voltage_mV / 1000.0;
  out.tds_raw_ppm = (TDS_CAL_COEFF_A * v * v * v) - (TDS_CAL_COEFF_B * v * v) + (TDS_CAL_COEFF_C * v);

  out.totalLiters = totalLitersAccum;
}
