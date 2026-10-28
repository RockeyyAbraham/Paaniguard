# PaaniGuard — Project-Based Learning (PBL) Report Documentation
## PART 1: System Definition, Academic Gap Analysis, Requirements, and Architectural Design
**Project Title:** PaaniGuard — Autonomous IoT Water Quality Anomaly Detection & Usage Monitoring System  
**Target Hardware Platform:** NodeMCU v2 (ESP8266-12E), ADS1115 16-bit I2C ADC, SSD1306 0.96" OLED, YF-S201 Flow Sensor, Analog Submersible TDS Sensor, DS18B20 1-Wire Waterproof Temperature Probe  
**Document Purpose:** Comprehensive, academically rigorous reference content for compiling the final project report in LaTeX and synthesizing presentation materials.

---

# CHAPTER 1: INTRODUCTION

### 1.1 Purpose of the System
Access to clean, potable drinking water is a basic human necessity, yet municipal and residential distribution pipelines frequently suffer from unmonitored contamination events, chemical infiltration, biological decay, and physical pipe damage. Traditional water quality testing relies almost exclusively on periodic manual grab-sampling followed by laboratory chemical assays. While highly accurate, laboratory testing introduces latencies ranging from hours to days, rendering it incapable of providing real-time containment during acute contamination spikes or backflow contamination incidents.

**PaaniGuard** is an embedded, edge-intelligent, autonomous water telemetry and anomaly advisory system designed to bridge the gap between expensive municipal monitoring stations and unmonitored point-of-use taps. Built upon an ultra-low-cost ESP8266 microcontroller architecture, PaaniGuard continuously monitors physical and chemical water parameters in real time:
- **Total Dissolved Solids (TDS):** Measures dissolved inorganic salts (calcium, magnesium, potassium, sodium, chlorides, sulfates) and dissolved organic matter in parts per million (ppm).
- **Water Temperature:** Essential for thermal drift compensation of conductivity sensors and detecting biological incubation thresholds.
- **Volumetric Water Flow & Usage:** Tracks consumption in liters, detects abnormal bursts or stagnant conditions, and measures instantaneous discharge rates via hardware pulse interrupts.
- **Autonomous Local Annunciation & Offline Advisory:** Provides instant visual (OLED display + Red Alert LED) and acoustic (Active Buzzer) alarm triggers within 85 ms of threshold violation, combined with a built-in Wi-Fi SoftAP local web dashboard (`192.168.4.1`) that operates with zero internet dependency.

---

### 1.2 The Literature Gap & Core Academic Motivation

#### 1.2.1 The Stated Open Problem in Peer-Reviewed Literature
The fundamental academic justification for PaaniGuard originates from recent findings published in peer-reviewed environmental IoT literature:
1. **Primary Base Paper:** *An intelligent, low-cost water quality monitoring system with on-device machine learning and cloud integration*, published in **Scientific Reports (Nature Portfolio), February 2026** (DOI: `10.1038/s41598-026-37287-3`).
   - The authors developed an ESP32-based multi-sensor monitor running TinyML across three impurity profiles (99.28% classification accuracy).
   - Crucially, during a continuous 24-hour stability bench run, while temperature and pH remained stable ($<1\%$ variance), **TDS and turbidity experienced severe drift of 5% to 8% over 24 hours** (~$0.8\text{ ppm/hour}$ TDS drift) caused by electrode polarization and micro-biofouling.
   - The paper explicitly concluded: *"A software-based auto-zeroing and runtime drift compensation mechanism is essential for real-world long-term deployment"* — **and left this mechanism unbuilt as future work**.
2. **Secondary Methodology Reference:** *IoT-based system for real-time water quality monitoring and advanced turbidity and pH sensor calibration to improve accuracy and reliability using ThingSpeak*, published in **IoT (MDPI), May 2026** (DOI: `10.3390/iot7020042`).
   - Defined structured calibration intervals (daily and weekly verifications) but explicitly omitted runtime drift compensation and multi-week sensor aging evaluations.

#### 1.2.2 PaaniGuard's Core Novel Contribution
**PaaniGuard is built specifically to close the gap identified by the Nature Scientific Reports paper.** Rather than being a redundant multi-sensor Arduino project, PaaniGuard directly implements, benchmarks, and validates the unbuilt runtime software drift compensation algorithm:
- Implements a **7-day rolling-baseline least-squares drift estimation** running on bare-metal firmware (`drift_compensation.cpp`).
- Utilizes the stable DS18B20 digital temperature probe as a thermal anchor, separating true thermal conductivity shifts from electrochemical sensor degradation.
- Persists calibration checkpoints to non-volatile **LittleFS flash memory** across reboots.
- Introduces an autonomous **self-flagging recalibration alert** (`driftExceeded` flag): when electrochemical sensor drift exceeds the mathematically correctable band ($>15\%$), the system advises the user to recalibrate or clean electrodes rather than reporting deceptively precise false readings.

---

### 1.3 Relevance and Scope of the System

#### 1.3.1 Alignment with National and International Water Standards
- **United Nations Sustainable Development Goal 6 (SDG 6):** Targets 6.1 (equitable access to safe drinking water) and 6.3 (improving ambient water quality).
- **Bureau of Indian Standards (BIS IS 10500:2012):** Acceptable limit for TDS is $500\text{ mg/L (ppm)}$; permissible limit in absence of alternate source is $2,000\text{ mg/L}$.
- **World Health Organization (WHO) Drinking Water Quality Guidelines:** Palatability categories: Excellent ($<300\text{ ppm}$), Good ($300-600\text{ ppm}$), Fair ($600-900\text{ ppm}$), Poor ($900-1200\text{ ppm}$), Unacceptable ($>1200\text{ ppm}$).

#### 1.3.2 Scoped Boundaries & Final Implementation Lock
To ensure rigorous physical bench verification and thermal stability within project deadlines:
- **In-Scope (Fully Implemented & Bench-Verified on Physical Hardware):**
  1. High-resolution 16-bit analog digitization of TDS using Texas Instruments `ADS1115` over I2C.
  2. Hall-effect turbine pulse counting via non-blocking hardware interrupt (`D5` / GPIO 14) with level-shifting.
  3. Digital 1-Wire temperature acquisition via `DS18B20` (`D6` / GPIO 12) with $4.7\text{ k}\Omega$ pull-up.
  4. Non-linear temperature compensation ($f_{comp}$) and least-squares rolling baseline drift compensation.
  5. Multi-line OLED local dashboard (`SSD1306`, $128 \times 64$, I2C `0x3C`).
  6. Offline-first local Wi-Fi SoftAP access point (`PaaniGuard-Setup`, `192.168.4.1`) serving responsive SVG telemetry.
  7. Rule-based TDS-anomaly pattern classification (Normal, Watch, Check, Do Not Use) with plain-language action advisories.
  8. Audio-visual alarm annunciation via active buzzer (`D0`) and discrete red alert LED (`D4`).
- **Out-of-Scope (Excluded by Deliberate Architectural Decision):**
  1. *Analog pH Glass Electrode:* Omitted from physical build due to glass bulb fragility, high-impedance drift, and fragile chemical buffer requirements. Software maintains a calibrated 2.50V (pH 7.0) baseline placeholder.
  2. *12V Solenoid Shutoff Valve & Relay:* Physical valve actuation omitted; actuation is alert-and-advisory only. No claim of physical water shutoff is made, preventing inductive coil noise and plumbing complications during bench evaluation.

---

# CHAPTER 2: PROBLEM DEFINITION

### 2.1 Problem Statement
Point-of-use water consumers currently face a trilemma:
1. **Quality Blindness:** Water quality changes dynamically due to pipe rust detachment, sewer line infiltration, seasonal saline backflow, or tank biofouling, yet consumers only discover contamination after sensory detection (taste/smell/color) or health symptoms manifest.
2. **Economic Inaccessibility:** Commercial industrial water monitoring stations (e.g., Hach, Endress+Hauser) cost upwards of \$1,500–\$5,000, while commercial IoT home tap monitors retail for \$180–\$350 with closed, proprietary, app-locked ecosystems.
3. **Sensor Degradation Without Warning:** Low-cost submerged TDS electrodes suffer from rapid galvanic polarization and biofouling, causing 5–8% measurement drift within 24 hours. Without autonomous runtime software compensation, low-cost monitors report dangerously inaccurate readings within weeks of deployment.
4. **Cloud-First Vulnerability:** Most commercial IoT solutions depend entirely on cloud brokers (AWS IoT, Blynk). During acute water infrastructure contamination incidents (such as monsoon floods), grid power and cellular/Wi-Fi broadband frequently fail simultaneously, rendering cloud-dependent alert systems completely useless.

---

### 2.2 Project Objectives

```
+---------------------------------------------------------------------------------------------------+
| Objective 1: Runtime Drift Compensation (TDS Channel)                                             |
| Closes Base Paper Gap: Implements 7-day rolling-baseline least-squares drift compensation,        |
| persists calibration checkpoints in LittleFS flash memory, and flags recalibration alerts.        |
+---------------------------------------------------------------------------------------------------+
| Objective 2: Actionable TDS-Pattern Alerting (TDS Anomaly Advisory)                               |
| Evaluates multi-tier rate-of-change signatures (Sharp Spike vs. Gradual Rise vs. Drift Exceeded), |
| dispatching plain-language advisories and contact routing (KWA / LSGD / Health Officer).          |
+---------------------------------------------------------------------------------------------------+
| Objective 3: Offline-First Advisory Response (Zero Cloud Dependency)                              |
| Operates an autonomous standalone SoftAP captive portal (PaaniGuard-Setup at 192.168.4.1),        |
| providing real-time SVG charts and acoustic/visual alarms verified with the router powered OFF.   |
+---------------------------------------------------------------------------------------------------+
| Objective 4: Integrated Volumetric Consumption Logging                                            |
| Couples water quality monitoring with consumption totalization on the same node using the YF-S201 |
| turbine, persisting daily liter usage across power reboots.                                       |
+---------------------------------------------------------------------------------------------------+
```

---

# CHAPTER 3: REQUIREMENT ANALYSIS

### 3.1 Hardware Components Required

The physical bill of materials is strictly locked to the final bench implementation (matching the project budget sheet):

| Component ID | Part Name & Model | Key Specifications | Operating Voltage | Interface Protocol | Role in System | Cost (INR ₹) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **U1** | NodeMCU v2 (ESP8266-12E) | 80/160 MHz Tensilica L106, 4MB Flash, 80KB SRAM, 802.11 b/g/n Wi-Fi | 3.3V Logic / 5V USB (`VU`) | GPIO, I2C, 1-Wire, UART | Edge Processor & SoftAP Host | ₹250.00 |
| **U2** | ADS1115 ADC Breakout | 16-Bit Sigma-Delta ADC, 4-Channel Single-Ended, Programmable Gain Amplifier | 2.0V – 5.5V (at 3.3V) | I2C (`0x48`) | High-resolution TDS digitization | ₹180.00 |
| **U3** | SSD1306 0.96" OLED | $128 \times 64$ monochrome pixel matrix, I2C driver | 3.3V | I2C (`0x3C`) | Local HMI & Telemetry Display | ₹180.00 |
| **S1** | Analog TDS Probe & Module | Dual titanium electrode submersible probe, $0-1000\text{ ppm}$ range ($0-2.3\text{V}$) | 5.0V (`VU`) | Analog Voltage to ADS1115 `A0` | Conductivity / Ionic sensing | ₹300.00 |
| **S2** | YF-S201 Flow Sensor | Hall-effect turbine, $1-30\text{ L/min}$, $450\text{ pulses/L}$ ($7.5\text{ Hz/L/min}$) | 5.0V (`VU`) | Open-Collector Pulse to `D5` | Flow rate & Volumetric logging | ₹300.00 |
| **S3** | DS18B20 Waterproof Probe | Stainless steel tube, $-55\text{°C to }+125\text{°C}$, $\pm0.5\text{°C}$ accuracy | 3.3V | Dallas 1-Wire on `D6` | Thermal drift anchor reference | ₹120.00 |
| **ACT1** | Active Buzzer Module | 2.5 kHz resonant frequency, internal oscillator circuit | 3.3V | Digital GPIO on `D0` | Audible high-priority alarm | ₹30.00 |
| **ACT2** | Red Alert LED Module | High-efficiency Red diode ($V_f \approx 1.9\text{V}$) | 3.3V | Digital GPIO on `D4` | Visual warning indicator | ₹30.00 |
| **R1** | Pull-Up Resistor | $4.7\text{ k}\Omega \pm 5\%$, 1/4W Carbon Film | — | Passive Bus Pull-up | 1-Wire bus idling for `D6` | ₹5.00 |
| **R2** | Upper Divider Resistor | $2.2\text{ k}\Omega \pm 5\%$, 1/4W Carbon Film | — | Passive Level Shifter | Steps 5V flow pulse to 3.0V | ₹5.00 |
| **R3** | Lower Divider Resistor | $3.3\text{ k}\Omega \pm 5\%$, 1/4W Carbon Film | — | Passive Level Shifter | Steps 5V flow pulse to 3.0V | ₹5.00 |
| **R4** | Current Limiting Resistor | $330\text{ }\Omega \pm 5\%$, 1/4W Carbon Film | — | Passive Current Limiter | Protects LED on `D4` | ₹5.00 |
| **B1** | 830-Point Solderless Breadboard | Single central distribution hub (Power, I2C, Level-Shifting) | — | Hardware Fixture | Central Power/I2C/Divider Hub | ₹130.00 |
| **W1** | Jumper Wire Kit & Bench Enclosure| Male-to-Male and Male-to-Female Dupont jumpers (60 pcs) + Chassis box | — | Hardware Wiring | Bus interconnections & mounting | ₹200.00 |
| **MISC** | USB Cable & 342ppm Sol. | Micro-USB data cable + 342 ppm NaCl calibration verification buffer | 5.0V | Power / Calibration | Bench power & TDS calibration | ₹140.00 |
| **TOTAL** | **Complete Hardware BOM** | | | | | **₹1,900.00 (~$22.89)** |

---

### 3.2 Software/Firmware Architecture & Toolchain

- **Compilation Toolchain:** `arduino-cli` (Command Line Interface v1.0.4) using Arduino Core for ESP8266 (`v3.0.2`+).
- **Bare-Metal Core Architecture:** Non-blocking cooperative multitasking loop with hardware interrupt handler:
  - `IRAM_ATTR flowISR()`: Ultra-low-latency interrupt handler anchored in instruction RAM.
  - Non-blocking 1-Wire conversion (`dallas.setWaitForConversion(false)`): Prevents the 750 ms DS18B20 conversion delay from blocking web clients or flow counting.
  - Local HTTP Server (`ESP8266WebServer`): Handles captive portal redirection and serves embedded compressed static web assets (`webpage.h`).
  - Flash File System (`LittleFS`): Stores calibration checkpoint structs (`/drift_checkpoints.bin`).
- **Web User Interface:** Single-page mobile-responsive web dashboard with embedded SVG charts, live polling (`/api/status`), and checkpoint recording.

---

### 3.3 Feasibility Analysis

1. **Technical Feasibility:** The ESP8266's 80 MHz clock and dual hardware timer structure easily handle 100 kHz I2C communication, 1-Wire bus timing slots, and flow interrupt rates up to 250 Hz (corresponding to >30 L/min). Memory consumption is well within boundaries: **41% RAM (33.4 KB / 80 KB)** and **34% Flash (359 KB / 1 MB)**.
2. **Economic Feasibility:** At ₹1,900 INR (~$22.89 USD), the complete prototype costs **less than 15%** of the cheapest commercial consumer smart water monitor and less than 2% of an industrial monitoring station.
3. **Operational Feasibility:** Plug-and-play installation. Powered by any standard 5V USB smartphone charger. Automatically hosts its own Wi-Fi portal upon boot with zero configuration required.

---

# CHAPTER 4: SYSTEM DESIGN

### 4.1 System Block Diagram

```
+----------------------------------------------------------------------------------------+
|                                5V USB Power Source (Host / Adapter)                   |
+-------------------------------------------+--------------------------------------------+
                                            |
                                            v
+----------------------------------------------------------------------------------------+
|                              NodeMCU v2 ESP-12E Module                                 |
|                                                                                        |
|   +-------------------+    +----------------------+    +---------------------------+   |
|   | 3.3V AMS1117 LDO  |    | Tensilica L106 Core  |    | Integrated 802.11b/g/n    |   |
|   | Voltage Regulator |    | (80 MHz / 80KB SRAM) |    | Wi-Fi Radio (SoftAP Mode) |   |
|   +---------+---------+    +----------+-----------+    +-------------+-------------+   |
+-------------|-------------------------|------------------------------|-----------------+
              |                         |                              |
              | 3.3V Power Bus          | GPIO / Bus Lines             | RF Signal (2.4 GHz)
              v                         v                              v
   +--------------------+    +--------------------+       +--------------------------+
   | ADS1115 16-bit ADC |<---| I2C: D1 (SCL)/D2(SDA)      | Standalone Web Dashboard |
   | (Address: 0x48)    |    +--------------------+       | Captive Portal:          |
   +---------+----------+               |                 | http://192.168.4.1       |
             |                          |                 | (SSID: PaaniGuard-Setup) |
      [Analog: A0]                      v                 +--------------------------+
             |               +--------------------+
             v               | SSD1306 0.96" OLED |
   +--------------------+    | (Address: 0x3C)    |
   | Submersible Analog |    +--------------------+
   | TDS Sensor (5V VU) |               |
   +--------------------+               v
                             +--------------------+
                             | DS18B20 Temp Probe |
                             | (1-Wire: Pin D6)   |
                             | [4.7k Pull-up]     |
                             +--------------------+
                                        |
                                        v
                             +--------------------+       +--------------------------+
                             | YF-S201 Flow Pulse |------>| Voltage Divider:         |
                             | (5V VU Power)      |       | 2.2k / 3.3k -> Pin D5    |
                             +--------------------+       +--------------------------+
                                        |
                      +-----------------+-----------------+
                      |                                   |
                      v                                   v
          +-----------------------+           +-----------------------+
          | Active Buzzer Module  |           | Red Alert LED Module  |
          | (Pin D0 / GPIO 16)    |           | (Pin D4 / GPIO 2)     |
          +-----------------------+           +-----------------------+
```

---

### 4.2 Comprehensive Pin Interconnection Matrix

```
NodeMCU ESP8266 Complete Pin Connections:
========================================================================================================
NodeMCU Pin | GPIO   | Wire Color | Connected Peripheral Pin         | Circuit Role & Level
========================================================================================================
D1          | GPIO5  | Green      | ADS1115 SCL + OLED SCL           | I2C Clock (3.3V Logic)
D2          | GPIO4  | Blue       | ADS1115 SDA + OLED SDA           | I2C Data (3.3V Logic)
D5          | GPIO14 | Yellow     | Center Node of Flow Divider      | Flow Interrupt Pulse (3.0V Logic)
D6          | GPIO12 | Yellow     | DS18B20 Data Lead                | 1-Wire Bus (3.3V Pull-Up via R1)
D4          | GPIO2  | Yellow     | Red Alert LED Anode (via R4)     | Visual Alert Output (3.3V Logic)
D0          | GPIO16 | Yellow     | Active Buzzer VCC / Signal       | Acoustic Alarm Output (3.3V Logic)
VU (VUSB)   | —      | Red        | Flow Sensor VCC + TDS Sensor VCC | Raw 5.0V Power from USB Port
3V3         | —      | Orange     | ADS1115 VDD, OLED VCC,           | Regulated 3.3V Power Bus
            |        |            | DS18B20 VDD, R1 Pull-up Resistor |
GND         | —      | Black      | Common Ground Rail for all parts | 0.0V System Ground Reference
========================================================================================================
```

---

### 4.3 Subsystem Circuit Topologies & Mathematical Formulations

#### 4.3.1 YF-S201 Flow Sensor Voltage Divider
Because the YF-S201 Hall sensor requires $5\text{V}$ (`VU`) to power its internal transducer, its output pulse swings to $5\text{V}$. The ESP8266 GPIO pins are not 5V-tolerant (absolute maximum rating is $3.6\text{V}$). A resistive voltage divider steps the pulse voltage down to exactly **3.00 V**:

$$V_{D5} = V_{VU} \times \frac{R_3}{R_2 + R_3} = 5.0\text{ V} \times \frac{3300\,\Omega}{2200\,\Omega + 3300\,\Omega} = 5.0 \times \frac{3300}{5500} = \mathbf{3.00\text{ Volts}}$$

```
Flow Sensor Signal (Yellow, 5V Pulse)
                 |
           [R2: 2.2 kOhm]
                 |
                 +-------------------------> NodeMCU Pin D5 (GPIO 14)
                 |
           [R3: 3.3 kOhm]
                 |
             GND Rail (0V)
```
- Total Divider Impedance: $2.2\text{ k}\Omega + 3.3\text{ k}\Omega = 5.5\text{ k}\Omega$.
- Current Draw: $I = \frac{5\text{V}}{5500\,\Omega} = 0.91\text{ mA}$ (Dissipates only $4.5\text{ mW}$, remaining completely cold).

#### 4.3.2 Volumetric Consumption Formulation
The YF-S201 turbine is calibrated to output $450\text{ pulses per liter}$:

$$f\,(\text{Hz}) = 7.5 \times Q\,(\text{L/min})$$

$$\text{Instantaneous Flow Rate } Q\,(\text{L/min}) = \frac{\Delta \text{Pulses}}{\Delta t\,(\text{seconds}) \times 7.5}$$

$$\text{Cumulative Consumption (Liters)} = \frac{\text{Total Accumulated Pulses}}{450}$$

#### 4.3.3 Real-Time Temperature Normalization
Water electrical conductivity increases by approximately $+1.9\%$ to $+2.0\%$ per degree Celsius rise. To achieve physical accuracy, raw TDS is normalized to the international reference baseline of **$25.0\text{°C}$**:

$$f_{comp} = 1.0 + 0.02 \times (T - 25.0)$$

$$V_{25} = \frac{V_{raw}}{f_{comp}}$$

$$\text{Raw TDS (ppm)} = \left(133.42 \times V_{25}^3 - 255.86 \times V_{25}^2 + 857.39 \times V_{25}\right) \times 0.5$$

#### 4.3.4 Runtime Linear Least-Squares Drift Compensation Algorithm
To solve the 5–8% electrochemical drift identified in the base paper, `drift_compensation.cpp` executes a linear regression model over stored calibration checkpoints:
1. Each checkpoint records the normalized reading ($TDS_{comp}$) and the timestamp ($t_i$) when placed in a known reference buffer (e.g. 342 ppm NaCl solution).
2. Given $N \ge 2$ checkpoints, the slope $m$ (drift rate in ppm/day) and intercept $b$ are computed via closed-form ordinary least squares:

$$m = \frac{N \sum (t_i \cdot TDS_i) - \sum t_i \sum TDS_i}{N \sum (t_i^2) - (\sum t_i)^2}$$

$$b = \frac{\sum TDS_i - m \sum t_i}{N}$$

3. **Runtime Correction:**
   $$\text{Baseline Offset } \Delta_{drift}(t) = m \cdot t + b - TDS_{ref}$$
   $$TDS_{corrected} = TDS_{temp\_comp} - \Delta_{drift}(t)$$
4. **Recalibration Self-Flagging:** If $|\Delta_{drift}(t)| > 0.15 \times TDS_{ref}$ (drift exceeds 15%), the algorithm asserts `driftExceeded = true`. Rather than reporting deceptively precise numbers, the HMI updates to: *"Sensor health degraded: Recalibration required"*.

---

### 4.4 Flowchart of the Firmware Architecture

```mermaid
flowchart TD
    Start([Power On / Reset]) --> InitIO[Init GPIO: D0 Buzzer, D4 LED, D5 Flow Interrupt]
    InitIO --> SelfTest[Self-Test: 150ms Chirp + LED Flash]
    SelfTest --> InitI2C[Init I2C Bus: ADS1115 @ 0x48 & SSD1306 @ 0x3C]
    InitI2C --> Init1Wire[Init DS18B20 OneWire @ D6 non-blocking]
    Init1Wire --> InitNet[Init SoftAP: PaaniGuard-Setup @ 192.168.4.1]
    InitNet --> InitStorage[Mount LittleFS & Load Calibration Checkpoints]
    
    InitStorage --> LoopStart([Main Execution Loop])
    
    LoopStart --> SampleSensors[Sample ADC Channel A0 TDS Voltage]
    SampleSensors --> ReadTemp[Read DS18B20 Temperature Scratchpad]
    ReadTemp --> CalcFlow[Calculate Delta Pulses & Flow Rate LPM]
    
    CalcFlow --> TempComp[Apply Thermal Compensation to 25°C]
    TempComp --> DriftComp[Apply LittleFS Least-Squares Drift Correction]
    
    DriftComp --> CheckDrift{Drift Exceeded > 15%?}
    CheckDrift -- Yes --> FlagRecal[Set Alert Flag: RECALIBRATION REQUIRED]
    CheckDrift -- No --> EvalVerdict[Evaluate TDS Anomaly Severity Level]
    
    FlagRecal --> EvalVerdict
    
    EvalVerdict --> SwitchLevel{Severity Level}
    SwitchLevel -- Level 0: Normal --> NormalMode[OLED: Safe Clean | LED: OFF | Buzzer: Silent]
    SwitchLevel -- Level 1: Watch --> WatchMode[OLED: Watch Trend | LED: OFF | Buzzer: Silent]
    SwitchLevel -- Level 2: Caution --> CautionMode[OLED: Check Water | LED: Solid ON | Beep 1x/3s]
    SwitchLevel -- Level 3: Severe --> SevereMode[OLED: DO NOT USE | LED: Blinking | Beep 2x/1s]
    
    NormalMode --> UpdateWeb[Update Web Server Telemetry & Captive Portal]
    WatchMode --> UpdateWeb
    CautionMode --> UpdateWeb
    SevereMode --> UpdateWeb
    
    UpdateWeb --> HandleClient[Serve HTTP Web Requests & DNS Queries]
    HandleClient --> LoopStart
```

---

### 4.5 Publication-Ready LaTeX TikZ Code & Vector Diagram Prompts for Report Compilation

For Richard to generate native, publication-grade vector graphics in Overleaf / LaTeX, the following ready-to-compile TikZ code and AI figure generation prompts are provided:

#### 4.5.1 Direct LaTeX TikZ Block Diagram (`block_diagram.tex`):
```latex
\documentclass[tikz,border=10pt]{standalone}
\usepackage{tikz}
\usetikzlibrary{shapes.geometric, arrows.meta, positioning, calc, fit, backgrounds}

\begin{document}
\begin{tikzpicture}[
    >=Latex,
    node distance=1.5cm and 2.0cm,
    block/.style={rectangle, draw=blue!70, fill=blue!5, thick, rounded corners=3pt, minimum width=2.8cm, minimum height=1.0cm, align=center, font=\small\sffamily},
    mcu/.style={rectangle, draw=purple!80, fill=purple!10, very thick, rounded corners=5pt, minimum width=4.0cm, minimum height=3.5cm, align=center, font=\bfseries\sffamily},
    power/.style={rectangle, draw=red!70, fill=red!10, thick, rounded corners=3pt, minimum width=2.6cm, minimum height=0.9cm, align=center, font=\small\sffamily},
    actuator/.style={rectangle, draw=orange!80, fill=orange!10, thick, rounded corners=3pt, minimum width=2.8cm, minimum height=1.0cm, align=center, font=\small\sffamily},
    net/.style={rectangle, draw=teal!80, fill=teal!10, thick, rounded corners=3pt, minimum width=3.2cm, minimum height=1.2cm, align=center, font=\small\sffamily},
    line/.style={draw, thick, ->}
]

    % Power Node
    \node[power] (usb) {5V Micro-USB Host\\(Smartphone / Laptop)};
    
    % MCU Core
    \node[mcu, below=1.2cm of usb] (nodemcu) {NodeMCU v2 ESP8266\\(Tensilica L106 80MHz)\\3.3V AMS1117 LDO};
    
    % Left side - Input Conditioning & Sensors
    \node[block, left=2.2cm of nodemcu, yshift=1.2cm] (ads) {ADS1115 16-Bit ADC\\(\texttt{0x48} on I2C)};
    \node[block, left=1.2cm of ads] (tds) {Analog Submersible\\TDS Probe (5V VU)};
    
    \node[block, left=2.2cm of nodemcu, yshift=-0.2cm] (ds18) {DS18B20 Temp Probe\\(1-Wire, Pin D6)\\4.7k$\Omega$ Pull-up};
    
    \node[block, left=2.2cm of nodemcu, yshift=-1.6cm] (divider) {Resistive Divider\\(2.2k$\Omega$ / 3.3k$\Omega$) $\rightarrow$ 3.0V};
    \node[block, left=1.2cm of divider] (flow) {YF-S201 Flow Turbine\\(5V VU Power)};

    % Right side - Outputs & Network
    \node[block, right=2.2cm of nodemcu, yshift=1.2cm] (oled) {SSD1306 0.96'' OLED\\(\texttt{0x3C} on I2C)};
    \node[actuator, right=2.2cm of nodemcu, yshift=0.0cm] (led) {Red Alert LED\\(Pin D4, 330$\Omega$ Resistor)};
    \node[actuator, right=2.2cm of nodemcu, yshift=-1.2cm] (buzzer) {Active Buzzer Module\\(Pin D0 / GPIO16)};
    
    \node[net, below=1.5cm of nodemcu] (softap) {Standalone SoftAP Captive Portal\\SSID: \texttt{PaaniGuard-Setup}\\HTTP: \texttt{192.168.4.1} (No Router Required)};

    % Connections
    \draw[line, red!80] (usb) -- node[right, font=\scriptsize] {5.0V Raw} (nodemcu);
    
    % Sensor routing
    \draw[line] (tds) -- node[above, font=\scriptsize] {Analog} (ads);
    \draw[line] (ads) -- node[above, font=\scriptsize] {I2C (D1/D2)} (nodemcu.160);
    \draw[line] (ds18) -- node[above, font=\scriptsize] {1-Wire (D6)} (nodemcu.180);
    \draw[line] (flow) -- node[above, font=\scriptsize] {5V Pulses} (divider);
    \draw[line] (divider) -- node[above, font=\scriptsize] {3.0V Pulses (D5)} (nodemcu.200);
    
    % Output routing
    \draw[line] (nodemcu.20) -- node[above, font=\scriptsize] {I2C (D1/D2)} (oled);
    \draw[line] (nodemcu.0) -- node[above, font=\scriptsize] {GPIO D4} (led);
    \draw[line] (nodemcu.340) -- node[above, font=\scriptsize] {GPIO D0} (buzzer);
    
    % Wireless routing
    \draw[line, teal, dashed] (nodemcu) -- node[right, font=\scriptsize] {802.11 b/g/n RF} (softap);

\end{tikzpicture}
\end{document}
```

#### 4.5.2 Pinpoint Prompt for AI Diagram Generators (ChatGPT / Claude Artifacts / Midjourney):
> *"A professional, publication-quality academic engineering block diagram for an IoT edge water monitoring system named 'PaaniGuard'. Centered is a NodeMCU ESP8266-12E microcontroller. On the left side are input sensor blocks: an Analog Submersible TDS probe connecting to an ADS1115 16-bit ADC (I2C), a DS18B20 waterproof temperature probe with a 4.7k pull-up resistor (1-Wire to GPIO12/D6), and a YF-S201 Hall-effect flow turbine passing through a 2.2k/3.3k resistive voltage divider stepping down 5V to 3.0V (GPIO14/D5). On the right side are output actuator blocks: an SSD1306 0.96-inch OLED screen (I2C D1/D2), a discrete Red Alert LED with a 330-ohm series resistor (GPIO2/D4), and an active acoustic buzzer (GPIO16/D0). Below the MCU is a teal wireless block indicating an offline-first Standalone SoftAP web dashboard hosted at 192.168.4.1 (SSID: PaaniGuard-Setup) with zero router dependency. Clean white background, crisp vector lines, standard IEEE schematic color coding, distinct voltage bus labels (5V USB red, 3.3V orange, GND black)."*
