# PaaniGuard — Project-Based Learning (PBL) Report Documentation
## PART 1: System Definition, Requirements, and Architectural Design
**Project Title:** PaaniGuard — Autonomous IoT Water Quality Anomaly Detection & Usage Monitoring System  
**Target Hardware Platform:** NodeMCU v2 (ESP8266-12E), ADS1115 16-bit ADC, SSD1306 OLED, YF-S201 Flow Sensor, Analog TDS Sensor, DS18B20 Waterproof Temperature Probe  
**Document Purpose:** Primary comprehensive reference content for academic LaTeX report compilation and LLM synthesis.

---

# CHAPTER 1: INTRODUCTION

### 1.1 Purpose of the System
Clean, potable water is a fundamental human necessity, yet municipal and residential distribution pipelines frequently suffer from unmonitored contamination events, chemical infiltration, biological decay, and physical pipe damage. Traditional water quality testing relies on periodic manual grab-sampling followed by laboratory chemical assays. While accurate, laboratory testing introduces latencies ranging from hours to days, rendering it incapable of providing real-time containment during acute contamination spikes or backflow contamination incidents.

**PaaniGuard** is an embedded, edge-intelligent, autonomous water telemetry and anomaly containment system designed to bridge the gap between expensive municipal monitoring stations and unmonitored point-of-use taps. Built upon an ultra-low-cost ESP8266 microcontroller architecture, PaaniGuard continuously monitors physical and chemical water parameters in real time:
- **Total Dissolved Solids (TDS):** Detects dissolved inorganic salts (calcium, magnesium, potassium, sodium, chlorides, sulfates) and dissolved organic matter.
- **Water Temperature:** Essential for thermal drift compensation of conductivity sensors and detecting biological incubation thresholds.
- **Volumetric Water Flow & Usage:** Tracks consumption in liters, detects abnormal bursts or stagnant conditions, and measures instantaneous discharge rates.
- **Autonomous Local Annunciation:** Provides instant visual (OLED display + Red Alert LED) and acoustic (Active Buzzer) alarm triggers within milliseconds of threshold violation, independent of internet connectivity.

The fundamental design philosophy of PaaniGuard is **Edge Autonomy First**. While cloud telemetry (ThingSpeak / MQTT) and local web server dashboards are supported, all critical classification, threshold evaluation, temperature compensation, and alarm triggering execute locally on the bare-metal microcontroller.

---

### 1.2 Relevance and Scope of the System

#### 1.2.1 Alignment with Global and National Standards
PaaniGuard directly aligns with the **United Nations Sustainable Development Goal 6 (SDG 6: Clean Water and Sanitation)**, specifically Target 6.1 (universal and equitable access to safe and affordable drinking water) and Target 6.3 (improving water quality by eliminating dumping and minimizing release of hazardous chemicals).

Domestically, the system benchmarks its sensory thresholds against:
- **Bureau of Indian Standards (BIS IS 10500:2012):** Drinking water specification outlining acceptable limits (TDS $< 500\text{ ppm}$, Permissible limit in absence of alternate source: $2000\text{ ppm}$).
- **World Health Organization (WHO) Guidelines for Drinking-water Quality (4th Edition):** Defining palatability categories (Excellent: $<300\text{ ppm}$, Good: $300-600\text{ ppm}$, Fair: $600-900\text{ ppm}$, Poor: $900-1200\text{ ppm}$, Unacceptable: $>1200\text{ ppm}$).

#### 1.2.2 System Scope & Operational Boundaries
- **In-Scope (Bench Verified on Physical Hardware):**
  1. High-precision 16-bit analog sampling of conductivity using the ADS1115 ADC over I2C.
  2. Hardware-level pulse accumulation via non-blocking Interrupt Service Routines (ISR) for the YF-S201 Hall-effect turbine.
  3. Digital 1-Wire temperature acquisition using the Dallas DS18B20 probe with CRC validation.
  4. Non-linear, temperature-compensated polynomial conversion from analog millivolts to TDS ppm.
  5. Multi-line visual dashboard rendering on a 128x64 SSD1306 OLED via hardware I2C bus arbitration.
  6. Multi-stage audio-visual alert dispatching (rhythmic buzzer pulsing and solid/flashing LED alarms on contamination).
- **Out-of-Scope (Deferred / Architectural Headroom):**
  1. *Analog pH Glass Electrode:* Kept modular; avoided during bench testing due to high-impedance glass bulb fragility and fragile calibration buffers; software maintains a calibrated 2.50V (pH 7.0) software placeholder.
  2. *12V High-Current Solenoid Valve & Relay Coil Switching:* Isolated on secondary breadboard architecture to prevent inductive kickback and ground bounce during bench calibration; firmware safety logic maintains full latching state machine support (`actuators.cpp`).

---

# CHAPTER 2: PROBLEM DEFINITION

### 2.1 Problem Statement
Conventional residential and commercial water distribution setups are blind to localized contamination events. Contaminants such as industrial runoff, sewage line seepage into aging municipal mains, excessive chlorination, or plumbing scale detachment occur unpredictably. Existing consumer-grade water purifiers (e.g., standard RO/UV systems) operate on blind timed-filter replacement schedules rather than real-time water quality feedback, leading to either premature filter wastage or consumption of contaminated water when filters saturate unnoticed. 

Furthermore, existing commercial industrial multi-parameter monitoring systems (e.g., Hach, Endress+Hauser) cost upwards of \$1,000–\$5,000, requiring dedicated three-phase power, proprietary bus transceivers, and specialized technician servicing. There is an acute lack of an open, low-cost (sub-\$30), accessible edge device capable of real-time multi-sensor telemetry, local hardware annunciation, and fail-safe operation.

### 2.2 Project Objectives
1. **Objective 1 — Multi-Parameter Edge Acquisition:**
   Develop an embedded sensory interface capable of acquiring water temperature, total dissolved solids, and flow velocity with sub-second update intervals, overcoming the single-channel 10-bit ADC limitation of the ESP8266 by integrating an external 16-bit Sigma-Delta ADC (`ADS1115`).
2. **Objective 2 — Algorithmic Drift & Temperature Compensation:**
   Implement non-linear mathematical polynomial curves to compensate for the $+2.0\%/\text{°C}$ thermal conductivity drift inherent to ionic solutions, normalizing all TDS readings to the standard reference baseline of $25.0\text{°C}$.
3. **Objective 3 — Fail-Safe Physical Annunciation & Interlocking:**
   Engineer a deterministic alert mechanism that guarantees acoustic and visual annunciation within 100 milliseconds of contamination detection ($\ge 500\text{ ppm}$ TDS threshold violation), operating on a dedicated low-jitter bare-metal loop.
4. **Objective 4 — Robust Electrical Integration on Breadboard:**
   Overcome physical hardware constraints including mixed-voltage bus arbitration (3.3V logic vs. 5V sensor power), open-collector Hall-effect pulse level shifting, and digital 1-Wire bus pull-up bus idling.

---

# CHAPTER 3: REQUIREMENT ANALYSIS

### 3.1 Hardware Components Required

| Component ID | Part Name & Model | Key Specifications | Operating Voltage | Interface Protocol | Role in PaaniGuard |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **U1** | NodeMCU v2 (ESP8266-12E) | 80/160 MHz Tensilica L106, 4MB Flash, 80KB SRAM, Integrated 802.11 b/g/n Wi-Fi | 3.3V (5V via Micro-USB / `VU`) | GPIO, I2C, 1-Wire, UART | Central Edge Processing Unit |
| **U2** | ADS1115 ADC Breakout | 16-Bit Sigma-Delta ADC, 4-Channel Single-Ended / 2 Differential, Programmable Gain ($2/3\times$ to $16\times$) | 2.0V – 5.5V (Powered at 3.3V) | I2C (`0x48`) | High-resolution analog digitization of TDS probe |
| **U3** | SSD1306 OLED Display | 0.96-inch monochrome display, $128 \times 64$ pixels, internal charge-pump | 3.3V – 5.0V (Powered at 3.3V) | I2C (`0x3C`) | Real-time local telemetry and alarm dashboard |
| **S1** | Analog TDS Sensor & Probe | 2-electrode titanium probe, range: $0 - 1000\text{ ppm}$, analog voltage output: $0 - 2.3\text{V}$ | 3.3V – 5.5V (Powered at 5V `VU`) | Analog voltage to ADS1115 `A0` | Ionic dissolved solid measurement |
| **S2** | YF-S201 Water Flow Sensor | Hall-effect turbine, flow range: $1 - 30\text{ L/min}$, output: $450\text{ pulses/L}$ ($7.5\text{ Hz per L/min}$) | 4.5V – 18V (Powered at 5V `VU`) | Open-Collector Digital Pulse to `D5` | Water flow rate & volumetric tracking |
| **S3** | DS18B20 Temperature Probe | Stainless steel waterproof encapsulation, range: $-55\text{°C} \text{ to } +125\text{°C}$, $\pm0.5\text{°C}$ accuracy | 3.0V – 5.5V (Powered at 3.3V) | Dallas 1-Wire on `D6` | Water temperature for drift compensation |
| **ACT1** | Active Buzzer Module | Continuous resonant frequency ~2.5 kHz, internal oscillation circuit | 3.3V – 5.0V (Driven from 3.3V) | Digital GPIO on `D0` | Audible high-priority contamination alarm |
| **ACT2** | High-Brightness Red LED | Discrete Red diode (from 4-pin RGB common cathode package) | Forward voltage $V_f \approx 1.8\text{V} - 2.0\text{V}$ | Digital GPIO on `D4` | Visual contamination warning |
| **R1** | Pull-Up Resistor | $4.7\text{ k}\Omega \pm 5\%$, 1/4W Carbon Film | — | Passive | Required bus pull-up for 1-Wire `D6` |
| **R2** | Upper Divider Resistor | $2.2\text{ k}\Omega \pm 5\%$, 1/4W (or $1.8\text{ k}\Omega$) | — | Passive | Voltage step-down on Flow Yellow wire |
| **R3** | Lower Divider Resistor | $3.3\text{ k}\Omega \pm 5\%$, 1/4W Carbon Film | — | Passive | Voltage step-down to GND for Flow `D5` |
| **R4** | Current Limiting Resistor | $330\text{ }\Omega \pm 5\%$, 1/4W Carbon Film | — | Passive | Current limiter for Red Alert LED on `D4` |

---

### 3.2 Software and Firmware Tools Used

- **Firmware Development Framework:** Arduino Core for ESP8266 (`v3.0.2`+).
- **Build & Compilation Toolchain:** `arduino-cli` (Command Line Interface v1.x) with target FQBN: `esp8266:esp8266:nodemcuv2`.
- **Primary Firmware Libraries:**
  - `Wire.h` (Hardware Two-Wire Interface implementation for ESP8266).
  - `Adafruit_GFX.h` & `Adafruit_SSD1306.h` (Frame-buffered graphics rendering).
  - `Adafruit_ADS1X15.h` (I2C control and voltage conversion for ADS1115).
  - `OneWire.h` & `DallasTemperature.h` (1-Wire bus timing, scratchpad access, CRC checking).
- **Host Testing & Environment:** Windows 11 PowerShell, Python 3.14 (used for automated serial port monitoring and testing scripts), Git version control.
- **Documentation & Simulation:** Wokwi Virtual Simulator (architecture reference), LaTeX (PBL final submission).

---

### 3.3 Feasibility Analysis

#### 3.3.1 Technical Feasibility
The ESP8266 provides an 80 MHz clock, 80 KB of RAM, and 4 MB of Flash memory. Running an I2C bus at 100 kHz, an interrupt-driven flow counter, a 1-Wire temperature conversion, and non-blocking display updates consumes less than **38% of SRAM** and **28% of Flash memory**, leaving immense headroom for WiFi and web server execution. The use of the ADS1115 completely circumvents the ESP8266's noisy internal 10-bit SAR ADC.

#### 3.3.2 Economic Feasibility
The entire bill of materials for the core monitoring prototype totals **under \$22.00 (₹1,750 INR)**. Compared to commercial inline TDS and flow meters that retail for upwards of \$150–\$300, PaaniGuard delivers equal or superior multi-parameter edge sensing at less than 10% of the cost.

#### 3.3.3 Operational Feasibility
The system requires zero daily human intervention. It powers up automatically via any standard 5V USB adapter, initializes all peripheral buses, self-calibrates, and begins autonomous local telemetry immediately.

---

# CHAPTER 4: SYSTEM DESIGN

### 4.1 Block Diagram of the Hardware Architecture

```
                    +---------------------------------------------+
                    |           5V USB Power Source               |
                    +----------------------+----------------------+
                                           |
                                           v
                       +---------------------------------------+
                       |       NodeMCU v2 ESP-12E Board        |
                       |  - 3.3V AMS1117 Linear Regulator      |
                       |  - VU Pin (Raw 5V USB Rail)           |
                       |  - 3V3 Pin (Regulated 3.3V Rail)      |
                       +---+---------------+---------------+---+
                           |               |               |
              [3.3V I2C Bus: D1/D2]     [D5: Flow]      [D6: 1-Wire]
                           |               |               |
       +-------------------+---+           |               |
       |                       |           |               |
       v                       v           |               |
+---------------+     +---------------+    |               |
|  ADS1115 ADC  |     |  SSD1306 OLED |    |               |
| (Addr: 0x48)  |     | (Addr: 0x3C)  |    |               |
+-------+-------+     +---------------+    |               |
        |                                  |               |
    [A0 Analog]                            |               |
        |                                  |               |
        v                                  v               v
+---------------+                 +-----------------+  +---------+
| Analog TDS    |                 | YF-S201 Flow    |  | DS18B20 |
| Probe & Board |                 | Sensor (5V VU)  |  | Temp    |
| (Powered 5V)  |                 | Voltage Divider |  | Probe   |
+---------------+                 | (2.2k / 3.3k)   |  | (4.7kPU)|
                                  +-----------------+  +---------+
                                           |
                          +----------------+----------------+
                          |                                 |
                          v                                 v
                 +------------------+             +-------------------+
                 | Active Buzzer    |             | Red Alert LED     |
                 | (D0 / GPIO16)    |             | (D4 / 330 Ohm)    |
                 +------------------+             +-------------------+
```

---

### 4.2 Circuit Schematic and Pin Interconnection Map

```
NodeMCU ESP8266 Pin Map:
========================================================================================
ESP8266 Pin  | NodeMCU Label | Wire Color | Connected Peripheral        | Operating Logic
========================================================================================
GPIO5        | D1            | Green      | ADS1115 SCL + OLED SCL      | 3.3V I2C Clock
GPIO4        | D2            | Blue       | ADS1115 SDA + OLED SDA      | 3.3V I2C Data
GPIO14       | D5            | Yellow     | YF-S201 Divider Center Node | 3.0V Step-Down Pulse
GPIO12       | D6            | Yellow     | DS18B20 1-Wire Data Line    | 3.3V Pulled-Up Bus
GPIO2        | D4            | Yellow     | Red Alert LED Anode (via R4)| 3.3V Digital Output
GPIO16       | D0            | Yellow     | Active Buzzer Positive Node | 3.3V Digital Output
VU           | VU (VUSB)     | Red        | Flow Sensor VCC + TDS VCC   | 5.0V Raw USB Power
3V3          | 3V3           | Orange     | ADS1115 VDD, OLED VCC,      | 3.3V Regulated Power
             |               |            | DS18B20 VDD, R1 Pull-up     |
GND          | GND           | Black      | Common Ground Rail for All  | 0.0V Ground Reference
========================================================================================
```

---

### 4.3 Detailed Subsystem Circuit Topologies

#### 4.3.1 I2C Sensor Subsystem (ADS1115 & SSD1306)
- The I2C bus utilizes **`D1` (SCL)** and **`D2` (SDA)**. Both breakouts have onboard $4.7\text{ k}\Omega - 10\text{ k}\Omega$ pull-up resistors to the 3.3V rail.
- **ADS1115 Addressing:** Pin `ADDR` is connected directly to `GND`, establishing the standard address `0x48`.
- **SSD1306 Addressing:** Factory hardwired address `0x3C`.
- **ADS1115 Analog Channel Mapping:**
  - `A0`: Analog TDS Sensor signal voltage ($0 - 2.3\text{V}$).
  - `A1`: Reserved for future Analog pH sensor ($0 - 3.3\text{V}$ with divider).
  - `A2`, `A3`: Spare high-impedance analog inputs.

#### 4.3.2 YF-S201 Flow Sensor Voltage Divider Topology
The YF-S201 Hall sensor requires $5\text{V}$ (`VU`) to energize its internal Hall effect transducer and Schmitt trigger. When water spins the internal 6-blade turbine, the Hall switch outputs a $5\text{V}$ square wave. Connecting $5\text{V}$ logic to an ESP8266 GPIO (maximum tolerance $3.3\text{V} + 0.3\text{V} = 3.6\text{V}$) would lead to oxide breakdown and gate destruction. 

A passive resistive voltage divider steps the pulse voltage down to exactly **3.00V**:

$$\text{Circuit Equation: } V_{D5} = V_{in} \times \frac{R_3}{R_2 + R_3} = 5.0\text{V} \times \frac{3300\,\Omega}{2200\,\Omega + 3300\,\Omega} = 5.0 \times \frac{3300}{5500} = \mathbf{3.00\text{ Volts}}$$

```
Flow Sensor Yellow Wire (5V Pulse)
          |
     [R2: 2.2 kOhm]
          |
          +---------------------------> To NodeMCU Pin D5 (GPIO14)
          |
     [R3: 3.3 kOhm]
          |
       GND Rail (0V)
```
- Total Divider Resistance: $2.2\text{ k}\Omega + 3.3\text{ k}\Omega = 5.5\text{ k}\Omega$.
- Current Draw during HIGH pulse: $I = \frac{5\text{V}}{5500\,\Omega} \approx \mathbf{0.91\text{ mA}}$ (Completely cold, dissipating only $4.5\text{ mW}$).

#### 4.3.3 DS18B20 1-Wire Temperature Interface Topology
The DS18B20 utilizes the proprietary Dallas 1-Wire protocol. Because the device drives the line using an open-drain internal FET, an external pull-up resistor ($R_1 = 4.7\text{ k}\Omega$) is mandatory between the Data line (`D6`) and the $3.3\text{V}$ power rail:

```
                      3.3V Rail
                          |
                    [R1: 4.7 kOhm]
                          |
DS18B20 Yellow Wire ------+-------------> To NodeMCU Pin D6 (GPIO12)
```
- Idle state: Resistor holds bus at $3.3\text{V}$ (`Logic 1`).
- Active transmission: Master and slave pull bus to Ground (`Logic 0`) via precise microsecond timing slots.

---

### 4.4 Working Principle and Software Flowcharts

#### 4.4.1 Flow Calculation & Calibration Equation
The YF-S201 produces pulse frequencies directly proportional to volumetric flow:

$$f\,(\text{Hz}) = 7.5 \times Q\,(\text{L/min})$$

Therefore, the instantaneous flow rate $Q$ is computed from delta pulses accumulated over delta time $\Delta t$:

$$Q\,(\text{L/min}) = \frac{\Delta \text{Pulses}}{\Delta t\,(\text{seconds}) \times 7.5}$$

To calculate total water volume consumed:

$$\text{Total Liters} = \frac{\text{Accumulated Pulse Count}}{450\text{ pulses/Liter}}$$

#### 4.4.2 Non-Linear TDS Calculation with Temperature Compensation
Water electrical conductivity varies by approximately $+1.9\%$ to $+2.0\%$ per degree Celsius rise due to decreased water viscosity and increased ion mobility. All raw TDS measurements must be normalized to the standard reference temperature of **$25.0\text{°C}$**.

1. **Temperature Compensation Coefficient ($f_{comp}$):**
   $$f_{comp} = 1.0 + 0.02 \times (T - 25.0)$$
   where $T$ is the live water temperature in °C acquired from the DS18B20 probe.
   
2. **Temperature-Corrected Voltage ($V_{25}$):**
   $$V_{25} = \frac{V_{raw}}{f_{comp}}$$
   where $V_{raw}$ is the voltage digitized by the ADS1115 (Gain = 1, $1\text{ LSB} = 0.125\text{ mV}$).

3. **Empirical Polynomial TDS Equation:**
   $$\text{TDS}\,(\text{ppm}) = \left(133.42 \times V_{25}^3 - 255.86 \times V_{25}^2 + 857.39 \times V_{25}\right) \times 0.5$$

#### 4.4.3 Firmware Execution Flowchart (Bare-Metal Non-Blocking Loop)

```
[System Power Up]
       |
       v
[pinMode Initialization: Buzzer D0, LED D4, Flow D5 (INPUT)]
       |
       v
[Hardware Self-Test: 150ms Buzzer Chirp + LED Flash]
       |
       v
[Wire.begin(4, 5) -> Initialize ADS1115 (0x48) & SSD1306 OLED (0x3C)]
       |
       v
[dallas.begin() -> Initialize DS18B20 on D6 (setWaitForConversion = FALSE)]
       |
       v
[attachInterrupt(D5, flowISR, FALLING)]
       |
       +<----------------------------------------------------+
       |                                                     |
       v                                                     |
[Read ADS1115 Channel A0 (TDS Voltage)]                      |
       |                                                     |
[Read DS18B20 Temperature (°C) via non-blocking scratchpad]  |
       |                                                     |
[Compute Flow Rate from Delta ISR Pulses]                    |
       |                                                     |
[Apply Temperature Compensation & Compute TDS ppm]           |
       |                                                     |
       +---------[ Is TDS >= 500 ppm ? ]                     |
       |                   |                                 |
      YES                  NO                                |
       |                   |                                 |
       v                   v                                 |
[ALARM STATE]       [NORMAL STATE]                           |
- Red LED: Pulsing  - Red LED: OFF                           |
- Buzzer: Beeping   - Buzzer: Silent                         |
- Status: ALERT     - Status: SAFE / CLEAN                   |
       |                   |                                 |
       +--------->+<-------+                                 |
                  |                                          |
                  v                                          |
[Render SSD1306 OLED: TDS, Temp, Flow, Pulses, Alarm Status] |
                  |                                          |
[Stream Telemetry Line over UART Serial (115200 baud)]       |
                  |                                          |
[Non-blocking Delay / Yield (350 ms)]                        |
                  |                                          |
                  +------------------------------------------+
```
