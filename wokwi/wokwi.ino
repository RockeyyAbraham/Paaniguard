/*
  PaaniGuard Wokwi target
  -----------------------
  This is an ESP32-only virtual hardware target. The production firmware in
  the repository root remains ESP8266 and is unchanged.

  TDS and pH stay synthetic because Wokwi has no ADS1115-based water probes
  in this project. The DS18B20, flow input, relay, buzzer, WS2812B, OLED,
  WiFi status page, drift correction, and fingerprinting are exercised here.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define PIN_DS18B20 12
#define PIN_FLOW 14
#define PIN_RELAY 13
#define PIN_WS2812B 2
#define PIN_BUZZER 16
#define OLED_SDA 4
#define OLED_SCL 5
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_I2C_ADDR 0x3C
#define SIM_DAY_LENGTH_MS 60000UL
#define TDS_REFERENCE_PPM 342.0
#define TDS_HIGH_THRESHOLD 600.0
#define PH_LOW_THRESHOLD 6.0
#define PH_HIGH_THRESHOLD 8.5
#define SENSOR_INTERVAL_MS 1000UL

OneWire oneWire(PIN_DS18B20);
DallasTemperature temperatureSensor(&oneWire);
Adafruit_NeoPixel statusLed(1, PIN_WS2812B, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
WebServer server(80);

volatile unsigned long flowPulses = 0;
unsigned long lastSensorMs = 0;
unsigned long simulationStartMs = 0;
unsigned long lastCalibrationMs = 0;
float driftSlope = 1.0;
float temperatureC = 25.0;
float flowRateLps = 0.0;
float correctedTds = TDS_REFERENCE_PPM;
float phValue = 7.0;

struct Alert {
  int severity;
  const char *message;
};

Alert currentAlert = {0, "Normal - no action needed"};

void IRAM_ATTR flowISR() {
  flowPulses++;
}

float readRawTDS() {
  float daysElapsed = (millis() - simulationStartMs) / (float)SIM_DAY_LENGTH_MS;
  float drift = daysElapsed * 15.0;
  float noise = random(-500, 500) / 100.0;
  unsigned long scenario = ((millis() - simulationStartMs) / 6000UL) % 4;
  if (scenario == 2) return 1100.0 + noise;
  if (scenario == 3) return 1200.0 + noise;
  return TDS_REFERENCE_PPM + drift + noise;
}

float readRawPH() {
  unsigned long scenario = ((millis() - simulationStartMs) / 6000UL) % 4;
  if (scenario == 2) return 5.0 + random(-20, 20) / 100.0;
  return 7.0 + random(-30, 30) / 100.0;
}

void runCalibrationCheck() {
  if (millis() - lastCalibrationMs < SIM_DAY_LENGTH_MS / 4) return;
  lastCalibrationMs = millis();
  float raw = readRawTDS();
  if (raw > 0.0) {
    driftSlope = TDS_REFERENCE_PPM / raw;
    Serial.printf("calibration: raw=%.1f driftSlope=%.4f\n", raw, driftSlope);
  }
}

Alert evaluatePattern(float tds, float ph) {
  if (tds > TDS_HIGH_THRESHOLD && ph < PH_LOW_THRESHOLD) {
    return {2, "SEVERE: high TDS + low pH; water shutoff triggered."};
  }
  if (tds > TDS_HIGH_THRESHOLD) {
    return {1, "CAUTION: TDS above safe threshold."};
  }
  if (ph < PH_LOW_THRESHOLD || ph > PH_HIGH_THRESHOLD) {
    return {1, "CAUTION: pH outside normal range."};
  }
  return {0, "Normal - no action needed"};
}

void applySafetyResponse(const Alert &alert) {
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_BUZZER, alert.severity == 2 ? HIGH : LOW);
  if (alert.severity == 2) statusLed.setPixelColor(0, statusLed.Color(255, 0, 0));
  else if (alert.severity == 1) statusLed.setPixelColor(0, statusLed.Color(255, 120, 0));
  else statusLed.setPixelColor(0, statusLed.Color(0, 80, 0));
  statusLed.show();
}

void renderDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.printf("PaaniGuard Wokwi\nTDS: %.0f ppm\npH: %.2f\nTemp: %.1f C\nFlow: %.2f L/s\nStatus: %s", correctedTds, phValue, temperatureC, flowRateLps, currentAlert.severity == 2 ? "SEVERE" : currentAlert.severity == 1 ? "CAUTION" : "OK");
  display.display();
}

void handleRoot() {
  String html = "<html><meta name='viewport' content='width=device-width'><body>";
  html += "<h2>PaaniGuard Wokwi</h2><p>TDS: " + String(correctedTds, 1) + " ppm</p>";
  html += "<p>pH: " + String(phValue, 2) + "</p><p>Temperature: " + String(temperatureC, 1) + " C</p>";
  html += "<p>Flow: " + String(flowRateLps, 2) + " L/s</p><h3>" + currentAlert.message + "</h3></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(34));
  simulationStartMs = millis();
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_FLOW, INPUT_PULLUP);
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), flowISR, FALLING);

  temperatureSensor.begin();
  statusLed.begin();
  statusLed.show();
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 8000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) Serial.println(WiFi.localIP());
  else Serial.println("WiFi unavailable; hardware simulation continues offline.");

  server.on("/", handleRoot);
  server.begin();
  lastCalibrationMs = millis();
  Serial.println("PaaniGuard Wokwi simulation ready");
}

void loop() {
  server.handleClient();
  if (millis() - lastSensorMs < SENSOR_INTERVAL_MS) return;
  lastSensorMs = millis();

  temperatureSensor.requestTemperatures();
  float measuredTemperature = temperatureSensor.getTempCByIndex(0);
  if (measuredTemperature > -100.0 && measuredTemperature < 125.0) temperatureC = measuredTemperature;

  noInterrupts();
  unsigned long pulses = flowPulses;
  flowPulses = 0;
  interrupts();
  flowRateLps = pulses / 7.5;

  runCalibrationCheck();
  float rawTds = readRawTDS();
  float temperatureFactor = 1.0 + 0.02 * (temperatureC - 25.0);
  correctedTds = (rawTds / temperatureFactor) * driftSlope;
  phValue = readRawPH();
  currentAlert = evaluatePattern(correctedTds, phValue);
  applySafetyResponse(currentAlert);
  renderDisplay();

  Serial.printf("Temp=%.1fC TDS=%.1fppm pH=%.2f Flow=%.2fL/s severity=%d\n", temperatureC, correctedTds, phValue, flowRateLps, currentAlert.severity);
}
