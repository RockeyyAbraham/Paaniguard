# PaaniGuard

ESP8266-based IoT water quality and usage monitoring system. Board target:
`esp8266:esp8266:nodemcuv2`. This firmware was built and compile-verified
with **no physical ESP8266 board attached** — see "What's stubbed pending
real hardware" and "Running without a board" below.

## File structure

| File | Purpose |
|---|---|
| `PaaniGuard.ino` | `setup()`/`loop()` only — wires the modules below together, no logic of its own |
| `config.h` | All pin definitions, WiFi placeholder credentials, thresholds, calibration constants |
| `sensors.h` / `.cpp` | Raw reads: pH & TDS (via ADS1115), DS18B20 temperature, flow pulse counting (ISR) |
| `drift_compensation.h` / `.cpp` | TDS temperature compensation + rolling-baseline drift correction, checkpoint persistence |
| `fingerprinting.h` / `.cpp` | Rule-based (non-ML) contamination pattern table and alert strings |
| `actuators.h` / `.cpp` | Relay/solenoid, buzzer, WS2812B status LED |
| `display.h` / `.cpp` | SSD1306 OLED rendering |
| `connectivity.h` / `.cpp` | WiFi station connect + SoftAP fallback |
| `webserver.h` / `.cpp` | Local status webpage (readings + active alert) |
| `cloud_logging.h` / `.cpp` | ThingSpeak push |
| `storage.h` / `.cpp` | LittleFS ring buffer of readings, survives reboot |
| `simulation.h` / `.cpp` | Synthetic sensor data for `SIMULATION_MODE` |

A separate `host_sim/` folder alongside this sketch (not part of the
firmware) is explained under "Running without a board".

## Pin map

Pins aren't locked to real hardware yet — this is a proposal, documented
here so it can be revisited once the board is on the bench.

| Signal | NodeMCU pin | GPIO | Why this pin |
|---|---|---|---|
| I2C SCL (ADS1115 + OLED) | D1 | GPIO5 | Standard ESP8266 Arduino core `Wire` default SCL |
| I2C SDA (ADS1115 + OLED) | D2 | GPIO4 | Standard ESP8266 Arduino core `Wire` default SDA |
| DS18B20 OneWire data | D6 | GPIO12 | No boot-time role; safe as a plain I/O from power-up |
| YF-S201 flow pulse (interrupt) | D5 | GPIO14 | Interrupt-capable and boot-safe; pulses can arrive before WiFi/cloud logic is up |
| Relay (solenoid valve) | D7 | GPIO13 | Plain output, no boot-time role; opto-isolation on the relay board protects the GPIO |
| WS2812B status LED data | D4 | GPIO2 | Requires HIGH at boot; its internal pull-up keeps it HIGH through boot since nothing writes to it until `setup()` runs |
| Buzzer | D0 | GPIO16 | No pull-up/interrupt support and normally reserved for deep-sleep wake (unused here); only ever written after `setup()` |
| *(unused, reserved)* | D3 | GPIO0 | Boot-sensitive (must read HIGH at boot) — left disconnected rather than risked |
| *(unused, reserved)* | D8 | GPIO15 | Boot-sensitive (must read LOW at boot) — left disconnected rather than risked |
| *(reserved)* | RX/TX | GPIO3/GPIO1 | Kept free for `Serial` — used for all debug/`SIMULATION_MODE` logging |
| *(intentionally unused)* | A0 | — | ESP8266's one onboard analog pin, only 10-bit — exactly why the ADS1115 is used instead (see below) |

**Why ADS1115 is mandatory:** the ESP8266 has exactly one analog input, at
10-bit resolution — not enough channels or precision for both pH and TDS.
The ADS1115 gives 4 channels at 16-bit over I2C, so pH is on ADS channel 0
and TDS is on ADS channel 1 (see `config.h`), with two channels spare.

## Libraries required

Install via `arduino-cli lib install`:

- Adafruit ADS1X15
- OneWire
- DallasTemperature
- Adafruit SSD1306
- Adafruit GFX Library
- Adafruit NeoPixel
- ThingSpeak
- (bundled with the `esp8266:esp8266` core: `ESP8266WiFi`, `ESP8266WebServer`, `LittleFS`)

**Cloud service: ThingSpeak**, not Blynk — a plain REST field-write needs no
persistent connection or companion app, and comfortably fits a
single-channel hobby/bench deployment. See `cloud_logging.cpp`.

## Opening / building this project

**Arduino IDE:** open `PaaniGuard.ino` directly (the IDE requires the sketch
folder and `.ino` file to share the same name, which they do here). Install
the ESP8266 board package (board manager URL
`https://arduino.esp8266.com/stable/package_esp8266com_index.json`) and the
libraries above, then select board "NodeMCU 1.0 (ESP-12E Module)".

**arduino-cli:**

```
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
arduino-cli lib install "Adafruit ADS1X15" "OneWire" "DallasTemperature" "Adafruit SSD1306" "Adafruit GFX Library" "Adafruit NeoPixel" "ThingSpeak"
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 PaaniGuard
```

Compiles clean: 0 errors, 0 warnings in any PaaniGuard file, with
`--warnings all`. (The bundled ThingSpeak library itself emits 2 unused-
parameter warnings from its own header — not this project's code — when
`--warnings all` is used.) Flash size ~323KB/1MB (31%), IRAM ~61KB/65KB
(93%, mostly the ESP8266 core's own reserved instruction cache).

## Running without a board

No ESP8266 is attached to this project yet, so two separate things stand in
for "flash it and watch Serial":

**1. `SIMULATION_MODE` in the actual firmware.** `config.h` has
`#define SIMULATION_MODE 1`. With it on, `sensors.cpp` calls
`simulation.cpp` instead of touching real hardware, cycling through four
scenarios (6 sensor-read ticks each, ~30s at the default 5s interval):
clean water, a backdated rolling-baseline drift check, and two
contamination fingerprints (pH-drop+TDS-spike, and TDS-severe-alone). Every
tick prints one Serial line with pH, raw TDS, corrected TDS, temperature,
flow, and the fingerprinting decision (severity + shutoff y/n). This is
real firmware code and compiles/would run exactly this way once flashed —
it just can't be captured here without a board plugged in.

**2. `host_sim/` — a native test harness for right now.** Since there's no
board to flash, `host_sim/` compiles the *actual, unmodified*
`drift_compensation.cpp` and `fingerprinting.cpp` against two tiny
stand-in headers (`Arduino.h`, `LittleFS.h` — just enough surface for
`Serial`, `millis()`, and an in-memory LittleFS) and runs the same four
scenarios as a plain Windows executable, so the drift-correction and
fingerprinting logic can be inspected today. Build and run it with:

```
cd host_sim
g++ -std=gnu++14 -I. main.cpp shim_globals.cpp ../drift_compensation.cpp ../fingerprinting.cpp -o paaniguard_sim.exe
./paaniguard_sim.exe
```

Sample output (captured from this exact build):

```
=== Scenario 1: normal/clean water sample ===
[clean@25C       ] pH= 7.00  rawTDS=  310.0  tempC=25.0  ->  tempCompTDS=  310.0  ->  baselineCorrectedTDS=  310.0
                     severity=NONE     shutoff=no  alert="Water quality within normal parameters."
[clean@30C       ] pH= 7.00  rawTDS=  310.0  tempC=30.0  ->  tempCompTDS=  281.8  ->  baselineCorrectedTDS=  281.8
                     severity=NONE     shutoff=no  alert="Water quality within normal parameters."

=== Scenario 2: rolling-baseline drift correction ===
recording 4 backdated calibration checkpoints (probe dipped in 342ppm reference), simulating a week of fouling drift:
drift_compensation: recorded checkpoint day=20697 measuredPpm=345.00
drift_compensation: recorded checkpoint day=20699 measuredPpm=351.00
drift_compensation: recorded checkpoint day=20701 measuredPpm=358.00
drift_compensation: recorded checkpoint day=20703 measuredPpm=364.00
[post-drift-fit  ] pH= 7.00  rawTDS=  310.0  tempC=25.0  ->  tempCompTDS=  310.0  ->  baselineCorrectedTDS=  287.9
                     severity=NONE     shutoff=no  alert="Water quality within normal parameters."

=== Scenario 3: contamination pattern A (pH drop + TDS spike) ===
[contam-A        ] pH= 5.00  rawTDS= 1100.0  tempC=25.0  ->  tempCompTDS= 1100.0  ->  baselineCorrectedTDS= 1077.9
                     severity=SEVERE   shutoff=YES alert="Combined pH drop/spike with elevated TDS detected. Strong signature of chemical or industrial contamination. Water supply automatically shut off as a precaution."

=== Scenario 4: contamination pattern B (TDS severe spike alone, pH normal) ===
[contam-B        ] pH= 7.00  rawTDS= 1200.0  tempC=25.0  ->  tempCompTDS= 1200.0  ->  baselineCorrectedTDS= 1177.9
                     severity=SEVERE   shutoff=YES alert="Severe TDS spike detected, independent of pH. Possible heavy contamination event (e.g. industrial discharge, saltwater intrusion). Water supply automatically shut off as a precaution."
```

What this demonstrates:
- **Temperature compensation** (mechanism 1): `clean@30C` shows the same
  310ppm raw reading normalized down to 281.8ppm at a warmer temperature —
  the ~2%/°C rule in `drift_applyTemperatureCompensation()`.
- **Rolling-baseline drift correction** (mechanism 2, distinct from the
  above): after 4 backdated checkpoints showing the probe reading
  progressively higher than the 342ppm reference, the *same* 310ppm raw
  input that scenario 1 called "310.0, no correction" is now corrected down
  to 287.9ppm — `drift_applyBaselineCorrection()`'s linear fit predicting
  and subtracting the accumulated drift.
- **Fingerprinting**: two distinct SEVERE patterns (pH+TDS combined, and
  TDS alone) both correctly trigger `shutoff=YES` with their own
  pre-written alert text, while the clean and drift scenarios correctly
  stay at `NONE`.

`host_sim/` is a verification aid only — it is not compiled or referenced
by the actual sketch, and arduino-cli never sees it.

## What's stubbed pending real hardware

Every value below is a **PLACEHOLDER** (also marked `// PLACEHOLDER` /
`// TODO` at its definition in `config.h`) and must be replaced once the
real sensors and board are on the bench:

- **WiFi credentials** (`WIFI_SSID`, `WIFI_PASSWORD`) — dummy values.
- **ThingSpeak channel/key** (`THINGSPEAK_CHANNEL_ID`, `THINGSPEAK_WRITE_API_KEY`).
- **pH calibration** (`PH_CALIBRATION_SLOPE`, `PH_CALIBRATION_OFFSET`) —
  must be derived from real readings in pH 4.0 and 7.0 buffer solutions.
- **TDS calibration** (`TDS_CAL_COEFF_A/B/C`, `TDS_VREF`) — must be
  re-fit against the 342ppm reference solution on the real ADS1115 + TDS
  module; current values are textbook-typical starting points only.
- **Flow sensor pulses-per-liter** (`FLOW_PULSES_PER_LITER`) —
  datasheet-typical for the YF-S201, must be bench-measured with a
  known-volume container (unit-to-unit variance is common).
- **Safety thresholds** (`PH_SAFE_MIN/MAX`, `PH_SEVERE_LOW/HIGH`,
  `TDS_SAFE_MAX_PPM`, `TDS_SEVERE_MAX_PPM`) — WHO-typical starting points,
  not tuned to any specific local water supply.
- **Local authority contact** (`LOCAL_AUTHORITY_CONTACT`) — placeholder
  text only; the firmware never auto-dials or auto-texts anyone regardless
  of what this string says.
- **SSD1306/ADS1115 I2C addresses** — common defaults, confirm once the
  actual modules are wired up (some SSD1306 boards ship at `0x3D`).

## Design notes worth knowing about

- **Valve fail-safe:** `PIN_RELAY_VALVE` defaults LOW at boot (relay
  de-energized, valve closed) and only opens when explicitly commanded — an
  unexpected reset never leaves water flowing unsupervised.
- **Offline-first safety:** `fingerprint_evaluate()` and
  `actuators_applyFingerprint()` never touch the network — the shutoff/
  buzzer/LED response is identical whether WiFi is connected, in SoftAP
  fallback, or fully offline.
- **No RTC:** `drift_getCurrentDayIndex()` uses NTP epoch time once online
  (synced opportunistically by `connectivity.cpp`), and falls back to a
  flash-persisted uptime-based day counter when offline. Any power-off
  downtime is treated as instantaneous in that fallback — a known,
  documented approximation until/unless a real RTC is added.
- **Storage never blocks or fails:** `storage.cpp`'s ring buffer is
  preallocated on first boot; every append overwrites one fixed slot, so
  there's no filesystem growth and no "disk full" failure mode.
