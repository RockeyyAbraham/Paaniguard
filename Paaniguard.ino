#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_ADS1115 ads;

#define PIN_ALERT_LED  PIN_WS2812_LED // D4 (GPIO2) - Red alert LED

bool oledOk = false;
bool adsOk = false;

volatile unsigned long flowPulses = 0;
unsigned long lastFlowTime = 0;
unsigned long lastPulses = 0;
float flowRateLPM = 0.0;

void IRAM_ATTR flowISR() {
  flowPulses++;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_ALERT_LED, OUTPUT);
  digitalWrite(PIN_ALERT_LED, LOW);

  // Quick power-on self test: 150ms chirp + flash
  digitalWrite(PIN_ALERT_LED, HIGH);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(150);
  digitalWrite(PIN_ALERT_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  // Flow sensor pin setup
  pinMode(PIN_FLOW_SENSOR, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_SENSOR), flowISR, FALLING);
  lastFlowTime = millis();

  Wire.begin(4, 5); // SDA=D2, SCL=D1
  Wire.setClockStretchLimit(1000);

  if (ads.begin(0x48)) {
    ads.setGain(GAIN_ONE);
    adsOk = true;
    Serial.println(F("ADS1115: OK"));
  } else {
    Serial.println(F("ADS1115: NOT FOUND"));
  }

  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oledOk = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(15, 25);
    display.print(F("PaaniGuard Active"));
    display.display();
    Serial.println(F("OLED: OK"));
  } else {
    Serial.println(F("OLED: NOT FOUND"));
  }
}

void loop() {
  float tdsV = 0.0;
  float tdsPpm = 0.0;

  if (adsOk) {
    int16_t raw0 = ads.readADC_SingleEnded(ADS1115_CHANNEL_TDS);
    tdsV = ads.computeVolts(raw0);
    float tdsComp = tdsV;
    tdsPpm = (133.42 * pow(tdsComp, 3) - 255.86 * pow(tdsComp, 2) + 857.39 * tdsComp) * 0.5;
    if (tdsPpm < 0) tdsPpm = 0;
  }

  bool isHighTds = (tdsPpm >= 500.0);

  // Compute flow rate every 500ms
  unsigned long now = millis();
  unsigned long elapsed = now - lastFlowTime;
  if (elapsed >= 500) {
    noInterrupts();
    unsigned long curPulses = flowPulses;
    interrupts();

    unsigned long delta = curPulses - lastPulses;
    lastPulses = curPulses;
    float dtSec = elapsed / 1000.0;
    lastFlowTime = now;
    flowRateLPM = (delta / dtSec) / 7.5; // YF-S201 calibration factor
  }

  // Red LED & Buzzer Alert
  if (isHighTds) {
    digitalWrite(PIN_ALERT_LED, HIGH); // Flash RED
    digitalWrite(PIN_BUZZER, HIGH);    // Beep buzzer
    delay(80);
    digitalWrite(PIN_ALERT_LED, LOW);
    digitalWrite(PIN_BUZZER, LOW);
  } else {
    digitalWrite(PIN_ALERT_LED, LOW);  // Red LED OFF
    digitalWrite(PIN_BUZZER, LOW);     // Buzzer OFF
  }

  // OLED Screen Update
  if (oledOk) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("== PAANIGUARD =="));

    display.setCursor(0, 12);
    display.print(F("TDS: "));
    display.print((int)tdsPpm);
    display.print(F(" ppm ("));
    display.print(isHighTds ? F("ALERT") : F("OK"));
    display.print(F(")"));

    display.setCursor(0, 26);
    display.print(F("Flow: "));
    display.print(flowRateLPM, 1);
    display.print(F(" L/min"));

    display.setCursor(0, 40);
    display.print(F("Pulses: "));
    display.print(flowPulses);

    display.setCursor(0, 53);
    display.print(F("Alert: "));
    display.print(isHighTds ? F("ALARM ON (RED)") : F("OFF (SAFE)"));

    display.display();
  }

  // Continuous Serial output
  Serial.print(F("LIVE_TDS: "));
  Serial.print(tdsPpm, 1);
  Serial.print(F(" ppm | Pulses: "));
  Serial.print(flowPulses);
  Serial.print(F(" | Rate: "));
  Serial.print(flowRateLPM, 1);
  Serial.print(F(" L/min | Status: "));
  Serial.println(isHighTds ? F("HIGH_TDS_ALERT") : F("NORMAL"));

  delay(350);
}
