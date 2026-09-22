# PaaniGuard Virtual Simulation

A browser-based logic and visualization test bench for PaaniGuard. This is a
separate project and does not modify or execute the production ESP8266
firmware.

## Boundaries

- Production source of truth: repository root `Paaniguard.ino`, `config.h`, and
  the root modules.
- Production board: NodeMCU ESP8266, `esp8266:esp8266:nodemcuv2`.
- This simulator: browser-based virtual hardware adapter.
- Existing `wokwi/`: separate experimental ESP32 project.
- Status: simulation only; it does not validate GPIO voltage, current,
  electrical isolation, relay ratings, sensor accuracy, or 12 V solenoid safety.

The adapter mirrors the production constants and behavior already present in
source: pH/TDS threshold classification, temperature compensation, drift
estimate, pH/TDS conversion curves, flow pulse placeholder, actuator colors,
buzzer behavior, safety shutoff state, OLED fields, web-style status, serial
output, and a practical in-memory storage/clock view. It intentionally labels
all analog values as simulated and not calibrated.

The production actuator code never issues an open-valve command in the current
application path. Therefore the simulator represents the relay output as LOW
and the virtual solenoid as CLOSED, including during a severe alert. This is
source-faithful rather than a new valve rule.

## Run

No build step or dependency install is required. Open [index.html](index.html)
in a browser, or serve this directory with any static file server:

```powershell
cd virtual-simulation
python -m http.server 8080
```

Then open `http://localhost:8080`.

## Controls

Manual mode controls temperature, pH, TDS, flow, drift, and noise. Scenario
mode provides normal, acidic, alkaline, high TDS, abnormal flow,
multi-parameter anomaly, and sensor drift cases. The simulation clock runs at
60 simulated seconds per real second and supports start, pause, and reset.

The dashboard exposes:

- Live readings and corrected TDS
- Virtual OLED preview
- WS2812B color state
- Buzzer state
- Relay and logical solenoid state
- ADS1115 address/channel view and virtual voltages
- Flow pulses, liters, and placeholder pulses-per-liter constant
- Serial-style output
- State event log
- Source-based manual test matrix
- Virtual architecture diagram marked as non-electrical

## Traceability

| Simulator element | Production source basis |
|---|---|
| pH/TDS thresholds | `config.h`, `fingerprinting.cpp` |
| pH/TDS conversion | `config.h`, `sensors.cpp` |
| temperature compensation | `config.h`, `drift_compensation.cpp` |
| drift estimate | `drift_compensation.cpp` |
| flow constant | `config.h`, `sensors.cpp` |
| LED/buzzer/relay behavior | `actuators.cpp` |
| OLED fields | `display.cpp` |
| serial fields | `Paaniguard.ino` |

The browser adapter is intentionally separate because the production modules
use Arduino and ESP8266 APIs that cannot run directly in a browser. It should
be updated alongside production logic changes and reviewed for behavioral
differences; it must never be used to claim physical hardware safety.
