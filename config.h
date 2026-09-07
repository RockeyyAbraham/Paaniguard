// config.h — PaaniGuard hardware/pin map, thresholds, calibration constants.
// All pin choices and threshold numbers below are documented individually.
// Anything marked PLACEHOLDER must be replaced with a bench-measured value
// once real hardware (sensors + ADS1115) is on the bench.

#ifndef PAANIGUARD_CONFIG_H
#define PAANIGUARD_CONFIG_H

// ---------------------------------------------------------------------------
// SIMULATION MODE
// Set to 1 to run entirely on synthetic sensor data (no hardware required).
// Set to 0 once real sensors are wired and calibrated.
// ---------------------------------------------------------------------------
#define SIMULATION_MODE 1

// ---------------------------------------------------------------------------
// PIN MAP (ESP8266 NodeMCU v2 / esp8266:esp8266:nodemcuv2)
//
// ESP8266 boot-strapping pins are GPIO0 (D3), GPIO2 (D4), GPIO15 (D8) and
// GPIO16 (D0). These pins carry a required logic level at power-up/reset
// (GPIO0 & GPIO2 must read HIGH, GPIO15 must read LOW) or behave specially
// (GPIO16 has no pull-up/interrupt support and is tied to deep-sleep wake).
// Driving them externally at the wrong level during boot can put the board
// into flash/programming mode instead of running the sketch, or block boot
// entirely. Per spec, these are only used here for signals that are never
// driven until after setup() runs (buzzer, status LED), or left completely
// unused. Nothing boot-sensitive is used for an input or an early output.
//
// I2C is fixed to the ESP8266 Arduino core's default Wire pins: D1 (GPIO5,
// SCL) and D2 (GPIO4, SDA). Both the ADS1115 ADC and the SSD1306 OLED share
// this single bus at their default I2C addresses (0x48 and 0x3C typically).
// ---------------------------------------------------------------------------

// -- I2C bus (ADS1115 + SSD1306 OLED) — standard NodeMCU I2C pins
#define PIN_I2C_SCL          5   // D1 / GPIO5  — Wire default SCL
#define PIN_I2C_SDA          4   // D2 / GPIO4  — Wire default SDA

// -- DS18B20 waterproof temperature probe (OneWire, single device)
// GPIO12 has no boot-time role and is free to use as a plain digital I/O
// from the moment the sketch starts, so it's a safe home for OneWire.
#define PIN_ONEWIRE_TEMP     12  // D6 / GPIO12 — OneWire bus data line

// -- YF-S201 flow sensor (hall-effect pulse output)
// Needs an interrupt-capable pin that is stable at boot (no bootstrapping
// role) since pulses can start arriving as soon as water flows, even before
// WiFi/cloud logic is up. GPIO14 fits both requirements.
#define PIN_FLOW_SENSOR      14  // D5 / GPIO14 — interrupt (FALLING edge)

// -- 12V solenoid valve, via opto-isolated relay board
// Plain digital output pin, no boot-time constraint. Opto-isolation on the
// relay board protects the ESP8266 GPIO from the 12V valve side.
#define PIN_RELAY_VALVE      13  // D7 / GPIO13 — HIGH = relay energized

// -- WS2812B addressable status LED (single pixel, bit-banged via
// Adafruit_NeoPixel). GPIO2 requires HIGH at boot; the ESP8266's internal
// pull-up on this pin combined with never writing to it until setup() runs
// keeps it naturally HIGH through boot, so it is safe for a boot-sensitive
// pin here specifically because it is a "do nothing until after boot" output.
#define PIN_WS2812_LED       2   // D4 / GPIO2  — NeoPixel data in

// -- Buzzer (simple active buzzer, digitalWrite HIGH/LOW only)
// GPIO16 has no pull-up/pull-down and no interrupt support and is normally
// reserved for deep-sleep wake; we don't use deep sleep, and the buzzer is
// only ever written to after setup() completes, so it's a safe, otherwise-
// idle home for this simple output-after-boot signal.
#define PIN_BUZZER           16  // D0 / GPIO16 — HIGH = buzzer on

// -- Reserved / intentionally unused
// D3 (GPIO0) and D8 (GPIO15) are left disconnected. Both carry a required
// boot-time logic level (HIGH and LOW respectively) and this design has
// enough other safe pins that there's no need to risk them.
// RX/TX (GPIO3/GPIO1) are kept free for Serial (used for debug logging and
// all SIMULATION_MODE output).
// A0 (the ESP8266's one onboard analog pin, 10-bit) is intentionally unused
// — this is exactly why the ADS1115 external ADC is used instead (see below).

// ---------------------------------------------------------------------------
// ADS1115 external ADC (I2C, 16-bit, 4 channels)
// Required because the ESP8266 has only one analog input at 10-bit
// resolution — not enough precision or channel count for pH + TDS.
// ---------------------------------------------------------------------------
#define ADS1115_I2C_ADDRESS  0x48  // default address (ADDR pin tied to GND)
#define ADS1115_CHANNEL_PH   0     // A0 on the ADS1115 — pH probe
#define ADS1115_CHANNEL_TDS  1     // A1 on the ADS1115 — TDS module
// A2, A3 on the ADS1115 are spare for future sensors.

// SSD1306 OLED
#define OLED_I2C_ADDRESS     0x3C  // common default for 0.96" SSD1306 modules
#define OLED_WIDTH           128
#define OLED_HEIGHT          64

// ---------------------------------------------------------------------------
// WiFi — PLACEHOLDER credentials. Replace with real network details.
// ---------------------------------------------------------------------------
#define WIFI_SSID            "PLACEHOLDER_SSID"      // TODO: real network SSID
#define WIFI_PASSWORD        "PLACEHOLDER_PASSWORD"  // TODO: real network password
#define WIFI_CONNECT_TIMEOUT_MS  15000  // how long to try station mode before SoftAP fallback

// SoftAP fallback (device becomes its own access point)
#define SOFTAP_SSID          "PaaniGuard-Setup"
#define SOFTAP_PASSWORD      "paaniguard123"  // TODO: consider requiring change on first boot
#define SOFTAP_LOCAL_IP      "192.168.4.1"    // ESP8266 SoftAP default, documented for clarity

// ---------------------------------------------------------------------------
// Cloud logging — ThingSpeak chosen over Blynk (see README for rationale:
// simpler REST push, no persistent connection/app required, free tier fits
// a single-channel hobby deployment).
// ---------------------------------------------------------------------------
#define THINGSPEAK_CHANNEL_ID     0            // PLACEHOLDER — TODO: real channel ID
#define THINGSPEAK_WRITE_API_KEY  "PLACEHOLDER_TS_WRITE_KEY"  // TODO: real write API key
#define THINGSPEAK_PUSH_INTERVAL_MS  60000     // don't hammer ThingSpeak's free-tier rate limit (15s min)

// ---------------------------------------------------------------------------
// pH calibration — PLACEHOLDER. Must be bench-calibrated with pH 4.0 and
// 7.0 buffer solutions before these numbers mean anything. The probe output
// is assumed linear in millivolts (from the ADS1115) between the two
// buffer points: pH = PH_SLOPE * voltage_mV + PH_OFFSET.
// ---------------------------------------------------------------------------
#define PH_CALIBRATION_SLOPE   -5.70   // PLACEHOLDER — TODO: derive from pH4/pH7 bench readings
#define PH_CALIBRATION_OFFSET  21.34   // PLACEHOLDER — TODO: derive from pH4/pH7 bench readings

// ---------------------------------------------------------------------------
// TDS calibration — PLACEHOLDER coefficients for the common analog TDS
// module's voltage->ppm curve (the widely-used DFRobot-style cubic fit).
// Must be re-derived against the 342 ppm reference solution on real
// hardware; these are textbook-typical starting values only.
// ---------------------------------------------------------------------------
#define TDS_VREF                 3.3    // ADS1115 reference assumption, PLACEHOLDER pending bench check
#define TDS_CAL_COEFF_A          133.42 // PLACEHOLDER — TODO: fit against 342ppm reference
#define TDS_CAL_COEFF_B          255.86 // PLACEHOLDER — TODO: fit against 342ppm reference
#define TDS_CAL_COEFF_C          857.39 // PLACEHOLDER — TODO: fit against 342ppm reference
#define TDS_REFERENCE_PPM        342.0  // known reference solution value, for checkpointing

// TDS temperature compensation (mechanism 1 of 2 — see drift_compensation.*)
// Standard ~2%/°C correction normalizing to a 25C reference.
#define TDS_TEMP_COMPENSATION_COEFF  0.02
#define TDS_TEMP_REFERENCE_C         25.0

// TDS rolling-baseline drift correction (mechanism 2 of 2 — distinct from
// temperature compensation above). Tracks calibration checkpoints (dips
// into the 342ppm reference) over a rolling window and fits a linear trend.
#define DRIFT_WINDOW_DAYS         7
#define DRIFT_MAX_CHECKPOINTS     14   // up to 2/day over the window

// ---------------------------------------------------------------------------
// Flow sensor (YF-S201) — PLACEHOLDER. Datasheet-typical pulses-per-liter;
// must be re-measured on the bench with a known-volume container once the
// real sensor is wired, since unit-to-unit variance is common.
// ---------------------------------------------------------------------------
#define FLOW_PULSES_PER_LITER   450.0  // PLACEHOLDER — TODO: bench-measure with known volume

// ---------------------------------------------------------------------------
// Contamination fingerprinting thresholds — PLACEHOLDER starting points.
// Rule-based (not ML): see fingerprinting.cpp for the lookup table these
// feed into.
// ---------------------------------------------------------------------------
#define PH_SAFE_MIN              6.5    // PLACEHOLDER — WHO-typical potable range
#define PH_SAFE_MAX              8.5    // PLACEHOLDER — WHO-typical potable range
#define TDS_SAFE_MAX_PPM         500.0  // PLACEHOLDER — WHO-typical potable limit
#define TDS_SEVERE_MAX_PPM       1000.0 // PLACEHOLDER — above this = severe regardless of pH
#define PH_SEVERE_LOW            5.5    // PLACEHOLDER — sharp acidic drop threshold
#define PH_SEVERE_HIGH           9.5    // PLACEHOLDER — sharp alkaline spike threshold

// Local authority contact — PLACEHOLDER. This is display/output text only;
// the firmware never auto-dials or auto-texts anyone.
#define LOCAL_AUTHORITY_CONTACT  "PLACEHOLDER: Contact your local water authority / municipal helpline"

// ---------------------------------------------------------------------------
// Local storage (circular buffer of readings, survives reboot)
// ---------------------------------------------------------------------------
#define STORAGE_MAX_RECORDS      200   // ring buffer capacity in LittleFS
#define STORAGE_FILE_PATH        "/log.bin"
#define STORAGE_CHECKPOINT_PATH  "/checkpoints.bin"

// ---------------------------------------------------------------------------
// Main loop timing
// ---------------------------------------------------------------------------
#define SENSOR_READ_INTERVAL_MS   5000
#define DISPLAY_REFRESH_INTERVAL_MS  2000

#endif // PAANIGUARD_CONFIG_H
