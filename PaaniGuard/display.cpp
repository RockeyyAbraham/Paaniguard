#include "display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool oledAvailable = false;

bool display_init() {
  oledAvailable = oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
  if (!oledAvailable) {
    Serial.println(F("display: SSD1306 not found at OLED_I2C_ADDRESS — skipping OLED output."));
    return false;
  }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.display();
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
  oled.setCursor(0, 0);

  oled.print(F("pH: "));
  oled.println(readings.ph_value, 2);

  oled.print(F("TDS: "));
  oled.print(correctedTdsPpm, 0);
  oled.println(F(" ppm"));

  oled.print(F("Temp: "));
  oled.print(readings.temperatureC, 1);
  oled.println(F(" C"));

  oled.print(F("Flow: "));
  oled.print(readings.flowRateLPM, 1);
  oled.println(F(" L/min"));

  oled.print(F("Status: "));
  oled.println(severityLabel(fingerprint.severity));

  if (fingerprint.severity != Severity::NONE) {
    // Word-wrapped by Adafruit_GFX's println at this text size only up to
    // the display width — long alert strings are intentionally truncated
    // here since the full text is always available over Serial/webserver.
    oled.println(fingerprint.alertMessage);
  }

  oled.display();
}
