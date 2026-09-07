#include "actuators.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel statusLed(1, PIN_WS2812_LED, NEO_GRB + NEO_KHZ800);
static bool valveOpen = false;

void actuators_init() {
  pinMode(PIN_RELAY_VALVE, OUTPUT);
  digitalWrite(PIN_RELAY_VALVE, LOW); // fail-safe closed at boot
  valveOpen = false;

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  statusLed.begin();
  statusLed.show(); // all pixels off
}

void actuators_setValve(bool open) {
  valveOpen = open;
  digitalWrite(PIN_RELAY_VALVE, open ? HIGH : LOW);
}

bool actuators_isValveOpen() {
  return valveOpen;
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

  if (result.triggerShutoff) {
    actuators_setValve(false);
  }
}
