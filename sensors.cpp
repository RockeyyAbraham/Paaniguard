// sensors.cpp — raw sensor reads. In SIMULATION_MODE, real hardware calls
// are replaced by simulation.cpp's synthetic generator (see config.h).

#include "sensors.h"
#include "config.h"
#include <Wire.h>

#if SIMULATION_MODE
#include "simulation.h"
#else
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>

static Adafruit_ADS1115 ads;
static OneWire oneWire(PIN_ONEWIRE_TEMP);
static DallasTemperature dallas(&oneWire);
#endif

static volatile unsigned long flowPulseCount = 0;
static unsigned long lastFlowCalcMillis = 0;
static float totalLitersAccum = 0.0;
#if !SIMULATION_MODE
static unsigned long lastFlowPulseSnapshot = 0;
static float lastFlowRateLPM = 0.0;
#endif

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
  // Wire is started here (not inside the #else below) because the I2C bus
  // is shared with the OLED (display.cpp), which is real hardware even
  // when SIMULATION_MODE fakes the pH/TDS/temperature/flow readings.
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

#if SIMULATION_MODE
  simulation_init();
#else
  ads.begin(ADS1115_I2C_ADDRESS);
  ads.setGain(GAIN_ONE);
  dallas.begin();
  dallas.setWaitForConversion(false);
  // pinMode(PIN_FLOW_SENSOR, INPUT_PULLUP);
  // attachInterrupt(digitalPinToInterrupt(PIN_FLOW_SENSOR), sensors_flowISR, FALLING);
#endif
  lastFlowCalcMillis = millis();
}

#if !SIMULATION_MODE
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
#endif

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
  int16_t tdsRaw = ads.readADC_SingleEnded(ADS1115_CHANNEL_TDS);
  out.tds_voltage_mV = ads.computeVolts(tdsRaw) * 1000.0;
  out.ph_voltage_mV = 2500.0;

  if (dallas.getDeviceCount() > 0) {
    dallas.requestTemperatures();
    float tc = dallas.getTempCByIndex(0);
    out.temperatureC = (tc < -55.0 || tc > 125.0) ? 25.0 : tc;
  } else {
    out.temperatureC = 25.0; // Default room temp reference when unattached
  }

  out.flowRateLPM = computeFlowRateLPM();
#endif

  // pH sensor omitted from hardware: use neutral safe value (7.0) to prevent false alerts
  out.ph_value = 7.0;

  // TDS standard curve
  float v = out.tds_voltage_mV / 1000.0;
  out.tds_raw_ppm = (133.42 * pow(v, 3) - 255.86 * pow(v, 2) + 857.39 * v) * 0.5;
  if (out.tds_raw_ppm < 0) out.tds_raw_ppm = 0;

  out.totalLiters = totalLitersAccum;
}
