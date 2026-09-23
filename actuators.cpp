#include "actuators.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel statusLed(1, PIN_WS2812_LED, NEO_GRB + NEO_KHZ800);
static bool valveOpen = false;
// Latch lives in RAM only, never in flash/RTC: a reset must land in the
// closed-and-unlatched state so the valve reopens under supervision rather
// than a stale latch keeping the household dry forever.
static bool shutoffLatched = false;
static uint8_t cleanReadingStreak = 0;

void actuators_init() {
  pinMode(PIN_RELAY_VALVE, OUTPUT);
  digitalWrite(PIN_RELAY_VALVE, RELAY_LEVEL_VALVE_CLOSED); // fail-safe closed at boot
  valveOpen = false;
  shutoffLatched = false;
  cleanReadingStreak = 0;

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  statusLed.begin();
  statusLed.show(); // all pixels off
}

void actuators_setValve(bool open) {
  valveOpen = open;
  digitalWrite(PIN_RELAY_VALVE, open ? RELAY_LEVEL_VALVE_OPEN : RELAY_LEVEL_VALVE_CLOSED);
}

bool actuators_isValveOpen() {
  return valveOpen;
}

bool actuators_isShutoffLatched() {
  return shutoffLatched;
}

void actuators_setBuzzer(bool on) {
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

void actuators_setStatusColor(uint8_t r, uint8_t g, uint8_t b) {
  statusLed.setPixelColor(0, statusLed.Color(r, g, b));
  statusLed.show();
}

void actuators_applyFingerprint(const FingerprintResult &result) {
  switch (result.severity) {
    case Severity::NONE:
      actuators_setStatusColor(0, 40, 0);   // dim green — all clear
      actuators_setBuzzer(false);
      break;
    case Severity::WATCH:
      actuators_setStatusColor(0, 40, 60);  // dim cyan — minor deviation
      actuators_setBuzzer(false);
      break;
    case Severity::WARNING:
      actuators_setStatusColor(80, 40, 0);  // amber — notable deviation
      actuators_setBuzzer(false);
      break;
    case Severity::SEVERE:
      actuators_setStatusColor(120, 0, 0);  // red — contamination fingerprint matched
      actuators_setBuzzer(true);
      break;
  }

  // One bad reading latches the valve shut; it takes a sustained run of
  // clean readings to undo that, because a single clean sample right after
  // contamination is far more likely to be a sensor recovering than the
  // water actually being safe again.
  if (result.triggerShutoff) {
    shutoffLatched = true;
    cleanReadingStreak = 0;
    actuators_setValve(false);
    return;
  }

  if (shutoffLatched) {
    if (cleanReadingStreak < VALVE_REOPEN_CLEAN_READINGS) {
      cleanReadingStreak++;
    }
    if (cleanReadingStreak < VALVE_REOPEN_CLEAN_READINGS) {
      return; // still latched — keep the water off
    }
    shutoffLatched = false;
    cleanReadingStreak = 0;
  }

  if (!valveOpen) {
    actuators_setValve(true); // normal operation established, let water flow
  }
}
