#include "display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool oledAvailable = false;

bool display_init() {
  Serial.println(F("display_init: starting..."));
  oledAvailable = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
  if (!oledAvailable) {
    Serial.println(F("display: SSD1306 not found at OLED_I2C_ADDRESS"));
    return false;
  }
  Serial.println(F("display_init: SSD1306 found, rendering welcome..."));
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(15, 25);
  oled.print(F("PaaniGuard Ready"));
  oled.display();
  Serial.println(F("display_init: complete."));
  return true;
}

static const char *severityLabel(Severity s) {
  switch (s) {
    case Severity::NONE:    return "OK";
    case Severity::WATCH:   return "WATCH";
    case Severity::WARNING: return "WARNING";
    case Severity::SEVERE:  return "ALERT";
  }
  return "?";
}

void display_render(const SensorReadings &readings, float correctedTdsPpm,
                     const FingerprintResult &fingerprint) {
  if (!oledAvailable) return;

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // Top Header Banner
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(F("== PAANIGUARD =="));

  // Status line
  oled.setCursor(0, 12);
  if (fingerprint.severity == Severity::SEVERE || fingerprint.severity == Severity::WARNING) {
    oled.print(F("Status: ! HIGH TDS !"));
  } else if (readings.tds_voltage_mV < 50.0) {
    oled.print(F("Status: PROBE IN AIR"));
  } else {
    oled.print(F("Status: SAFE / CLEAN"));
  }

  // Big TDS Number
  oled.setCursor(0, 24);
  oled.setTextSize(2);
  oled.print((int)correctedTdsPpm);
  oled.setTextSize(1);
  oled.print(F(" ppm"));

  // Voltage row
  oled.setCursor(0, 44);
  oled.setTextSize(1);
  oled.print(F("Voltage: "));
  oled.print(readings.tds_voltage_mV / 1000.0, 3);
  oled.print(F(" V"));

  // Temperature row
  oled.setCursor(0, 55);
  oled.print(F("Temp: "));
  if (readings.temperatureC <= -55.0 || readings.temperatureC >= 125.0) {
    oled.print(F("-- C"));
  } else {
    oled.print(readings.temperatureC, 1);
    oled.print(F(" C"));
  }

  oled.display();
}
