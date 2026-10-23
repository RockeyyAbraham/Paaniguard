#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include <ESP8266WiFi.h>
#include "connectivity.h"
#include "webserver.h"
#include "fingerprinting.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_ADS1115 ads;

#define PIN_ALERT_LED  PIN_WS2812_LED // D4 (GPIO2) - Red alert LED

// 1 = feed simulated TDS and flow (30 s cycle: normal, rising, check, do not
// use, recovery). Everything downstream is the real code path: verdict, buzzer,
// LED, OLED, web page, history. Set to 0 for real sensors.
#define DEMO_SENSOR_DATA 0
// -1 = cycle through all states. 0, 2 or 3 = hold that state (0 normal and
// silent, 2 check your water, 3 do not use) so the demo can be paused.
#define DEMO_HOLD_LEVEL -1

bool oledOk = false;
bool adsOk = false;
bool tdsReady = false;   // a TDS value is available (real ADC, or simulated data)

volatile unsigned long flowPulses = 0;
unsigned long lastFlowTime = 0;
unsigned long lastPulses = 0;
float flowRateLPM = 0.0;

void IRAM_ATTR flowISR() {
  flowPulses++;
}

#if DEMO_SENSOR_DATA
// Simulated scenario. Six 5 s phases (30 s cycle), linearly interpolated between the
// start and end value of each phase.
static void demoSample(float &ppm, float &lpm, float &tempC) {
  tempC = 25.0 + 2.5 * sin(millis() / 3000.0);   // drifts around room temperature, visible within one cycle
#if DEMO_HOLD_LEVEL >= 0
  {
    static const float holdPpm[4] = {150, 150, 640, 1180};
    static const float holdLpm[4] = {2.4, 2.4, 1.8, 0.0};
    ppm = holdPpm[DEMO_HOLD_LEVEL] + sin(millis() / 700.0) * 4.0;
    lpm = holdLpm[DEMO_HOLD_LEVEL];
    return;
  }
#endif
  static const float ppmA[6] = {150, 160, 640, 700, 1180, 1100};
  static const float ppmB[6] = {160, 640, 700, 1180, 1100, 150};
  static const float lpmA[6] = {2.4, 2.4, 2.2, 1.8, 0.0, 0.0};
  static const float lpmB[6] = {2.4, 2.2, 1.8, 0.0, 0.0, 2.4};
  const unsigned long PHASE_MS = 5000;
  unsigned long t = millis() % (6 * PHASE_MS);
  int p = t / PHASE_MS;
  float f = (t % PHASE_MS) / (float)PHASE_MS;
  ppm = ppmA[p] + (ppmB[p] - ppmA[p]) * f + sin(millis() / 700.0) * 4.0;
  lpm = lpmA[p] + (lpmB[p] - lpmA[p]) * f;
  if (ppm < 0) ppm = 0;
  if (lpm < 0.02) lpm = 0;
}

// Inverse of the TDS polynomial in loop() (monotonic for v >= 0), so the
// simulated probe voltage shown on the page matches the simulated ppm.
static float demoPpmToVolts(float ppm) {
  float lo = 0.0, hi = 3.3;
  for (int i = 0; i < 20; ++i) {
    float v = (lo + hi) / 2;
    float p = (133.42 * v * v * v - 255.86 * v * v + 857.39 * v) * 0.5;
    if (p < ppm) lo = v; else hi = v;
  }
  return (lo + hi) / 2;
}
#endif

// Same wording as the web page's verdicts.
static const char *const LEVEL_LABEL[4] = {
  "Water looks normal", "Keep an eye on it", "Check your water", "Do not use"
};

// Time-based patterns (no delay), called every few ms from loop().
//   check your water (2): LED solid, one short beep every 3 s
//   do not use (3):       LED blinking, double beep every second
// Normal and watch stay silent.
static void alertOutputs(int level) {
  unsigned long ms = millis();
  bool led = false, buzz = false;
  if (level == 2) {
    led = true;
    buzz = (ms % 3000) < 80;
  } else if (level >= 3) {
    led = (ms % 500) < 250;
    unsigned long p = ms % 1000;
    buzz = (p < 80) || (p >= 160 && p < 240);
  }
  digitalWrite(PIN_ALERT_LED, led ? HIGH : LOW);
  digitalWrite(PIN_BUZZER, buzz ? HIGH : LOW);
}

// 128x64 version of the web page: verdict header (inverted when attention is
// needed, like the page's coloured hero), big TDS number, flow and litres,
// and the same normal/raised/severe scale with a marker at the reading.
static void drawOled(int level, float ppm, float lpm, float litres, float tempC, bool tempOk) {
  if (!oledOk) return;
  display.clearDisplay();
  bool inv = tdsReady && level >= 2;
  if (inv) display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  display.setTextColor(inv ? SSD1306_BLACK : SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(2, 1);
  if (tdsReady) display.print(LEVEL_LABEL[level]);
  else       display.print(F("No TDS reading"));

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(3);
  display.setCursor(0, 14);
  if (tdsReady) display.print((int)ppm);
  else       display.print(F("--"));
  display.setTextSize(1);
  display.setCursor(display.getCursorX() + 3, 30);
  display.print(F("ppm"));

  // Water temperature, top right. "--" until a probe is fitted.
  display.setCursor(98, 14);
  display.print(F("Temp"));
  display.setCursor(98, 24);
  if (tempOk) {
    display.print(tempC, 1);
    display.print(F("C"));
  } else {
    display.print(F("--"));
  }

  display.setCursor(0, 40);
  display.print(F("Flow "));
  display.print(lpm, 1);
  display.print(F(" L/min"));
  display.setCursor(92, 40);
  display.print(litres, 1);
  display.print(F("L"));

  const float scaleMax = TDS_SEVERE_MAX_PPM * 1.5;
  display.drawRect(0, 53, 128, 5, SSD1306_WHITE);
  display.drawFastVLine((int)(TDS_SAFE_MAX_PPM / scaleMax * 127), 53, 5, SSD1306_WHITE);
  display.drawFastVLine((int)(TDS_SEVERE_MAX_PPM / scaleMax * 127), 53, 5, SSD1306_WHITE);
  if (tdsReady) {
    int mx = constrain((int)(ppm / scaleMax * 127), 1, 126);
    display.fillRect(mx - 1, 50, 3, 11, SSD1306_WHITE);
  }
  display.display();
}

// Shown while no phone is connected, so the join details are on the device.
static void drawOledConnect() {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);  display.print(F("Join this WiFi"));
  display.setCursor(0, 14); display.print(F(SOFTAP_SSID));
  display.setCursor(0, 28); display.print(F("Password"));
  display.setCursor(0, 40); display.print(F(SOFTAP_PASSWORD));
  display.setCursor(0, 54); display.print(F("Page opens by itself"));
  display.display();
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

  // Own WiFi network + status page. A phone that joins is sent to the page.
  connectivity_startSoftAP();
  webserver_init();
#if DEMO_SENSOR_DATA
  webserver_setDemo(true);
  webserver_setHistoryInterval(1000);   // chart fills in two minutes instead of an hour
  Serial.println(F("DEMO_SENSOR_DATA: TDS and flow are simulated"));
#endif
}

void loop() {
  float tdsV = 0.0;
  float tdsPpm = 0.0;
  float tempC = 25.0;      // water temperature; stays 25 until a probe is fitted
  bool tempOk = false;     // true once a real (or simulated) temperature reading exists

#if DEMO_SENSOR_DATA
  float demoLpm = 0.0;
  demoSample(tdsPpm, demoLpm, tempC);
  tempOk = true;
  tdsV = demoPpmToVolts(tdsPpm);
  tdsReady = true;

  // Turn the simulated flow into pulses, so the real flow-rate code below runs.
  static unsigned long lastDemoMs = millis();
  static float demoPulseFrac = 0.0;
  unsigned long demoNow = millis();
  demoPulseFrac += demoLpm * 7.5 * (demoNow - lastDemoMs) / 1000.0;
  lastDemoMs = demoNow;
  unsigned long demoWhole = (unsigned long)demoPulseFrac;
  demoPulseFrac -= demoWhole;
  noInterrupts();
  flowPulses += demoWhole;
  interrupts();
#else
  tdsReady = adsOk;
  if (adsOk) {
    int16_t raw0 = ads.readADC_SingleEnded(ADS1115_CHANNEL_TDS);
    tdsV = ads.computeVolts(raw0);
    float tdsComp = tdsV;
    tdsPpm = (133.42 * pow(tdsComp, 3) - 255.86 * pow(tdsComp, 2) + 857.39 * tdsComp) * 0.5;
    if (tdsPpm < 0) tdsPpm = 0;
  }
#endif

  bool isHighTds = (tdsPpm >= 500.0);

  // One verdict drives the beeper, LED, OLED and the web page, so they always agree.
  // level: 0 normal, 1 watch, 2 check your water, 3 do not use.
  FingerprintResult fp = fingerprint_evaluate(7.0, tdsPpm);
  int level = (int)fp.severity;

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

  // LED and buzzer are driven by alertOutputs() in the wait loop below, so
  // their patterns keep time without blocking the web server.

  // OLED: same layout as the web page. Only when no phone is connected, the
  // join-the-WiFi screen takes over for 3 s out of every 15 s.
  if (WiFi.softAPgetStationNum() == 0 && (millis() / 3000) % 5 == 0) {
    drawOledConnect();
  } else {
    drawOled(level, tdsPpm, flowRateLPM, flowPulses / FLOW_PULSES_PER_LITER, tempC, tempOk);
  }

  // Continuous Serial output
  Serial.print(F("LIVE_TDS: "));
  Serial.print(tdsPpm, 1);
  Serial.print(F(" ppm | Pulses: "));
  Serial.print(flowPulses);
  Serial.print(F(" | Rate: "));
  Serial.print(flowRateLPM, 1);
  Serial.print(F(" L/min | Status: "));
  Serial.print(isHighTds ? F("HIGH_TDS_ALERT") : F("NORMAL"));
  Serial.print(F(" | Level: "));
  Serial.print(level);
  Serial.print(F(" | Temp: "));
  if (tempOk) Serial.println(tempC, 1);
  else        Serial.println(F("--"));

  // Web page / API: share the same readings the OLED and serial just used.
  SensorReadings r = {};
  r.tds_voltage_mV = tdsV * 1000.0;
  r.tds_raw_ppm = tdsPpm;
  r.ph_value = 7.0;        // pH probe removed from scope, neutral value (as in sensors.cpp)
  r.temperatureC = tempC;
  r.flowRateLPM = flowRateLPM;
  r.totalLiters = flowPulses / FLOW_PULSES_PER_LITER;
  r.timestamp = millis();
  webserver_setSensorStatus(tdsReady, flowPulses > 0, tempOk, false);
  webserver_updateStatus(r, tdsPpm, fp, flowPulses);

  // Keep serving web requests for the rest of the loop period.
  unsigned long waitStart = millis();
  while (millis() - waitStart < 350) {
    alertOutputs(tdsReady ? level : 0);
    webserver_handleClient();
    delay(5);
  }
}
