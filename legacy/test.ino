/*
  PaaniGuard — ESP8266 Firmware (Wokwi build)
  ---------------------------------------------
  WHAT'S REAL vs SIMULATED IN THIS VERSION:
    - WiFi (station + SoftAP fallback) ............ REAL
    - DS18B20 temperature .......................... REAL (Wokwi has this part)
    - Flow sensor (pulse counting) ................. REAL (works if wired; reads 0 if not)
    - Relay / Buzzer / WS2812B / OLED .............. REAL
    - TDS and pH .................................... SIMULATED (no Wokwi part exists for
      either, and you don't have the physical sensors yet). Synthetic values drift over
      time on purpose so you can actually watch the drift-compensation logic work.
    - Drift compensation + fingerprinting ........... REAL logic, running on whatever TDS/pH
      values are current (simulated now, real later — same code path either way).

  SWAPPING IN REAL SENSORS LATER:
    Set USE_REAL_ANALOG_SENSORS to 1, wire an ADS1115, and replace the two lines inside
    readRawTDS() / readRawPH() marked "REAL SENSOR GOES HERE" with actual ADS1115 reads.
    Everything downstream (compensation, fingerprinting, alerts) needs zero changes.

  LIBRARIES NEEDED (add via Wokwi's Library Manager tab, or Arduino Library Manager):
    OneWire, DallasTemperature, Adafruit NeoPixel, Adafruit SSD1306, Adafruit GFX Library
    (Adafruit ADS1X15 only if/when you flip USE_REAL_ANALOG_SENSORS to 1)
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- CONFIG ----------------

#define WIFI_SSID "Wokwi-GUEST"      // Wokwi's built-in simulated network. Replace with your
#define WIFI_PASSWORD ""              // real WiFi credentials once this runs on real hardware.
#define WIFI_CONNECT_TIMEOUT_MS 8000  // how long to try station mode before falling back to SoftAP

#define SOFTAP_SSID "PaaniGuard-Alert"
#define SOFTAP_PASSWORD ""            // open network — matches your "no login needed" design goal

#define PIN_DS18B20      D5
#define PIN_FLOW         D6
#define PIN_RELAY        D7
#define PIN_WS2812B      D4
#define PIN_BUZZER       D0

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_I2C_ADDR 0x3C

#define USE_REAL_ANALOG_SENSORS 0     // flip to 1 once ADS1115 + real TDS/pH are wired in

// Drift-compensation timing. Compressed for simulation — a real deployment uses
// 7 real days; here we use minutes so you can actually watch it happen in one sitting.
#define SIM_DAY_LENGTH_MS   60000UL   // 1 "simulated day" = 60 seconds
#define TRIAL_LENGTH_DAYS   7

// Reference concentration used for calibration checkpoints (matches your real 342 ppm solution)
#define TDS_REFERENCE_PPM   342.0

// Thresholds used by the fingerprinting rules (placeholder values — tune once you have
// real calibrated data; these exist so the logic path is exercised end-to-end now)
#define TDS_HIGH_THRESHOLD   600.0
#define PH_LOW_THRESHOLD     6.0
#define PH_HIGH_THRESHOLD    8.5

// ---------------- GLOBAL OBJECTS ----------------

OneWire oneWire(PIN_DS18B20);
DallasTemperature tempSensor(&oneWire);

Adafruit_NeoPixel statusLED(1, PIN_WS2812B, NEO_GRB + NEO_KHZ800);

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

ESP8266WebServer server(80);

bool apMode = false;

volatile unsigned long flowPulseCount = 0;
unsigned long lastFlowCalcMs = 0;
float currentFlowRateLps = 0.0;   // litres per second, rough estimate

// Drift-compensation state
float driftSlope = 1.0;     // multiplicative correction factor, starts at "no correction"
float driftOffset = 0.0;    // additive correction, starts at "no correction"
unsigned long simStartMs = 0;

struct Alert {
  int severity;         // 0 = normal, 1 = caution, 2 = severe
  String message;
};

Alert currentAlert = {0, "Normal — no action needed"};
float lastCorrectedTDS = 0;
float lastPH = 7.0;
float lastTempC = 25.0;

// ---------------- FLOW SENSOR ISR ----------------

void IRAM_ATTR flowISR() {
  flowPulseCount++;
}

// ---------------- WIFI ----------------

void setupWiFi() {
  Serial.println("Attempting station connection...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    Serial.println();
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Station connect failed — falling back to SoftAP.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SOFTAP_SSID, SOFTAP_PASSWORD);
    apMode = true;
    Serial.print("SoftAP started. Connect to: ");
    Serial.println(SOFTAP_SSID);
    Serial.print("Local page at: http://");
    Serial.println(WiFi.softAPIP());
  }
}

void handleRoot() {
  String html = "<html><head><title>PaaniGuard</title></head><body>";
  html += "<h2>PaaniGuard Status</h2>";
  html += "<p>Mode: " + String(apMode ? "Offline (SoftAP)" : "Online") + "</p>";
  html += "<p>Temperature: " + String(lastTempC, 1) + " C</p>";
  html += "<p>TDS (corrected): " + String(lastCorrectedTDS, 1) + " ppm</p>";
  html += "<p>pH: " + String(lastPH, 2) + "</p>";
  html += "<p>Flow rate: " + String(currentFlowRateLps, 3) + " L/s</p>";
  html += "<h3>Alert (severity " + String(currentAlert.severity) + ")</h3>";
  html += "<p>" + currentAlert.message + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ---------------- SIMULATED SENSOR READS ----------------
// Replace the marked lines with real ADS1115 reads once hardware arrives.

float readRawTDS() {
#if USE_REAL_ANALOG_SENSORS
  // REAL SENSOR GOES HERE: read ADS1115 channel, convert voltage -> raw TDS formula
  return 0.0;
#else
  // Synthetic drift: starts near the true reference value, drifts upward over
  // "simulated days" to mimic real electrode fouling, so drift-compensation has
  // something real to correct.
  float daysElapsed = (millis() - simStartMs) / (float)SIM_DAY_LENGTH_MS;
  float driftAmount = daysElapsed * 15.0;               // +15 ppm drift per sim-day
  float noise = (float)(random(-500, 500)) / 100.0;      // +/- 5 ppm sensor noise
  return TDS_REFERENCE_PPM + driftAmount + noise;
#endif
}

float readRawPH() {
#if USE_REAL_ANALOG_SENSORS
  // REAL SENSOR GOES HERE: read ADS1115 channel, convert voltage -> pH formula
  return 7.0;
#else
  float noise = (float)(random(-30, 30)) / 100.0;        // +/- 0.3 pH noise
  return 7.0 + noise;
#endif
}

// ---------------- DRIFT COMPENSATION ----------------
// Periodically "dip the reference solution" (simulated) and update the correction
// model. This mirrors the real 7-day rolling-baseline approach at compressed speed.

unsigned long lastCalibrationCheckMs = 0;
#define CALIBRATION_CHECK_INTERVAL_MS (SIM_DAY_LENGTH_MS / 4)  // 4 checks per sim-day

void runCalibrationCheck() {
  if (millis() - lastCalibrationCheckMs < CALIBRATION_CHECK_INTERVAL_MS) return;
  lastCalibrationCheckMs = millis();

  float rawAtReference = readRawTDS();  // what the (drifted) sensor currently reads
  if (rawAtReference <= 0) return;

  // Single-point scale correction: what factor brings this reading back to the
  // known true reference value?
  driftSlope = TDS_REFERENCE_PPM / rawAtReference;

  Serial.print("[Calibration check] raw=");
  Serial.print(rawAtReference);
  Serial.print(" -> new driftSlope=");
  Serial.println(driftSlope, 4);
}

float applyTempCompensation(float rawTDS, float tempC) {
  // Standard ~2%/C compensation, normalized to 25C reference
  float compensationCoefficient = 1.0 + 0.02 * (tempC - 25.0);
  return rawTDS / compensationCoefficient;
}

float getCorrectedTDS() {
  float raw = readRawTDS();
  float tempCompensated = applyTempCompensation(raw, lastTempC);
  return (tempCompensated * driftSlope) + driftOffset;
}

// ---------------- FINGERPRINTING (rule-based) ----------------

Alert evaluatePattern(float tds, float ph) {
  if (tds > TDS_HIGH_THRESHOLD && ph < PH_LOW_THRESHOLD) {
    return {2, "SEVERE: High TDS + low pH detected — pattern consistent with sewage "
               "ingress. Water shutoff triggered. Contact local water authority."};
  }
  if (tds > TDS_HIGH_THRESHOLD) {
    return {1, "CAUTION: TDS above safe threshold — possible sediment/pipe disturbance. "
               "Avoid drinking until levels normalize."};
  }
  if (ph < PH_LOW_THRESHOLD || ph > PH_HIGH_THRESHOLD) {
    return {1, "CAUTION: pH outside normal range. Monitor closely."};
  }
  return {0, "Normal — no action needed"};
}

// ---------------- ACTUATORS ----------------

void applySafetyResponse(const Alert &alert) {
  if (alert.severity == 2) {
    digitalWrite(PIN_RELAY, HIGH);  // trigger shutoff
    digitalWrite(PIN_BUZZER, HIGH);
    statusLED.setPixelColor(0, statusLED.Color(255, 0, 0)); // red
  } else if (alert.severity == 1) {
    digitalWrite(PIN_RELAY, LOW);
    digitalWrite(PIN_BUZZER, LOW);
    statusLED.setPixelColor(0, statusLED.Color(255, 150, 0)); // amber
  } else {
    digitalWrite(PIN_RELAY, LOW);
    digitalWrite(PIN_BUZZER, LOW);
    statusLED.setPixelColor(0, statusLED.Color(0, 255, 0)); // green
  }
  statusLED.show();
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("PaaniGuard  ");
  display.println(apMode ? "[OFFLINE]" : "[ONLINE]");
  display.print("Temp: "); display.print(lastTempC, 1); display.println(" C");
  display.print("TDS:  "); display.print(lastCorrectedTDS, 0); display.println(" ppm");
  display.print("pH:   "); display.println(lastPH, 2);
  display.print("Flow: "); display.print(currentFlowRateLps, 2); display.println(" L/s");
  display.println(currentAlert.severity == 0 ? "Status: OK" :
                   currentAlert.severity == 1 ? "Status: CAUTION" : "Status: SEVERE");
  display.display();
}

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("PaaniGuard booting...");

  randomSeed(analogRead(A0));
  simStartMs = millis();

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_FLOW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), flowISR, FALLING);

  tempSensor.begin();

  statusLED.begin();
  statusLED.show();

  Wire.begin(D2, D1); // SDA, SCL
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("OLED not found — continuing without display.");
  } else {
    display.clearDisplay();
    display.display();
  }

  setupWiFi();
  server.on("/", handleRoot);
  server.begin();

  lastCalibrationCheckMs = millis();
  Serial.println("Setup complete.\n");
}

// ---------------- LOOP ----------------

unsigned long lastLoopMs = 0;

void loop() {
  server.handleClient();

  if (millis() - lastLoopMs < 1000) return;  // run main cycle once per second
  lastLoopMs = millis();

  // --- Temperature ---
  tempSensor.requestTemperatures();
  float t = tempSensor.getTempCByIndex(0);
  if (t != DEVICE_DISCONNECTED_C) lastTempC = t;

  // --- Flow rate (pulses/sec -> rough L/s, calibrate pulses-per-litre once real sensor is in) ---
  noInterrupts();
  unsigned long pulses = flowPulseCount;
  flowPulseCount = 0;
  interrupts();
  currentFlowRateLps = pulses / 7.5; // 7.5 is YF-S201's typical pulses-per-second-per-L/min constant; re-derive with real hardware

  // --- Drift compensation ---
  runCalibrationCheck();
  lastCorrectedTDS = getCorrectedTDS();
  lastPH = readRawPH();

  // --- Fingerprinting + safety response ---
  currentAlert = evaluatePattern(lastCorrectedTDS, lastPH);
  applySafetyResponse(currentAlert);
  updateDisplay();

  // --- Serial debug output ---
  Serial.print("Temp="); Serial.print(lastTempC, 1);
  Serial.print("C  TDS="); Serial.print(lastCorrectedTDS, 1);
  Serial.print("ppm  pH="); Serial.print(lastPH, 2);
  Serial.print("  Flow="); Serial.print(currentFlowRateLps, 2);
  Serial.print("L/s  Alert(sev="); Serial.print(currentAlert.severity);
  Serial.print("): "); Serial.println(currentAlert.message);
}
