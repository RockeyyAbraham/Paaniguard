// display.h — OLED (SSD1306, I2C) rendering of current readings + any
// active alert. Shares the I2C bus with the ADS1115 (see config.h).

#ifndef PAANIGUARD_DISPLAY_H
#define PAANIGUARD_DISPLAY_H

#include <Arduino.h>
#include "sensors.h"
#include "fingerprinting.h"

// Returns false if the SSD1306 didn't respond at OLED_I2C_ADDRESS (e.g. not
// wired up yet) — callers should not treat this as fatal, just skip
// display_render() calls.
bool display_init();

void display_render(const SensorReadings &readings, float correctedTdsPpm,
                     const FingerprintResult &fingerprint);

#endif // PAANIGUARD_DISPLAY_H
