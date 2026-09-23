# PaaniGuard Build Manual

**Set 8266-01** · Smart Water Quality & Usage Monitor
Builders: 4 · Sessions: ~6 · Difficulty: first hardware build

---

## How to read this manual

Steps are grouped into **BAGS**. Finish a bag, pass its **TEST GATE**, then open the next.
Do not skip ahead. A bag that fails its gate is faster to fix than four bags stacked on top
of a fault.

```
  [ ] P1  x1        parts needed for this step only
  ------------------------------------------------
  the action, in one line
  ------------------------------------------------
  ASCII diagram of the connection
```

- `[ ]` = tick it when done
- **STOP** = do not proceed until the check passes
- **!** = this step can damage hardware if done wrong
- Explanations are **not** in the steps. They live in [Appendix A](#appendix-a--why).

---

## Parts inventory

Tick each one as you find it. Do this before session 1, not during it.

### Boards & modules

| ID | Part | Qty | Have |
|----|------|-----|------|
| P1 | NodeMCU 1.0 ESP-12E (CH340) | 1 | [ ] |
| P2 | ADS1115 16-bit ADC breakout | 1 | [ ] |
| P3 | OLED 0.96" SSD1306 I2C, 4-pin | 1 | [ ] |
| P4 | DS18B20 waterproof probe | 1 | [ ] |
| P5 | YF-S201 flow sensor | 1 | [ ] |
| P6 | Relay module, 1-channel, opto-isolated | 1 | [ ] |
| P7 | Active buzzer module | 1 | [ ] |
| P8 | WS2812B addressable LED | 1 | [ ] |
| P9 | 12 V solenoid valve, 1/2", normally closed | 1 | [ ] |
| P10 | 12 V 2 A DC adapter | 1 | [ ] |
| P11 | Analog pH module + BNC probe | 1 | [ ] **NOT YET** |
| P12 | Analog TDS module + probe | 1 | [ ] **NOT YET** |

### Passives

| ID | Part | Qty | Used in | Have |
|----|------|-----|---------|------|
| R1 | 4.7 kOhm resistor | 1 | DS18B20 pull-up | [ ] |
| R2 | 1.8 kOhm resistor | 1 | Flow divider, upper | [ ] |
| R3 | 3.3 kOhm resistor | 1 | Flow divider, lower | [ ] |
| R4 | 330 Ohm resistor | 1 | WS2812B data | [ ] |
| C1 | 470 uF electrolytic, >= 6.3 V | 1 | WS2812B power | [ ] |
| D1 | 1N4007 diode | 1 | Solenoid flyback | [ ] |

### Consumables

| ID | Part | Qty | Have |
|----|------|-----|------|
| B1 | Breadboard, 830-point | 2 | [ ] |
| W1 | Jumper wires, male-to-female | ~25 | [ ] |
| W2 | Jumper wires, male-to-male | ~15 | [ ] |
| W3 | Jumper wires, female-to-female | ~5 | [ ] |
| U1 | USB micro cable, short and thick | 1 | [ ] |
| A1 | Barrel-jack-to-screw-terminal adapter | 1 | [ ] |
| PB1 | Perfboard (for BAG 8) | 1 | [ ] |

### Wire colour code

Use these colours throughout. It makes every later check faster.

| Colour | Carries |
|--------|---------|
| Red | 5 V |
| Orange | 3.3 V |
| Black | Ground |
| Blue | I2C SDA |
| Green | I2C SCL |
| Yellow | Signal / data |
| Brown | 12 V |

---

## BAG 0 — Before you build

### Step 1

```
  [ ] P2  ADS1115
```

Check whether the ADS1115's pin header is **soldered on** or loose in the bag.

If loose: it must be soldered before BAG 2. Arrange that now. Nothing grips an unsoldered
header.

### Step 2

```
  [ ] P1  NodeMCU     [ ] U1  USB cable
```

Plug the NodeMCU into a laptop. Install the CH340 driver if the port does not appear.

### Step 3

```
  [ ] P1  NodeMCU
```

Flash a blink sketch. Nothing else connected.

> **STOP — TEST GATE 0**
> The onboard LED blinks. If not, the problem is driver, cable or board — and it is not
> a wiring problem, because there is no wiring yet. Fix it here.

---

## BAG 1 — Power rails

### Step 4

```
  [ ] P1  NodeMCU     [ ] B1  Breadboard x1
```

Seat the NodeMCU across the centre channel of breadboard 1.

**!** The USB end holds the WiFi antenna. Position the board so that end **overhangs the
breadboard edge**.

```
     breadboard edge
          |
          v
    +-----------------------+
    |  [=== NodeMCU ===]    |     <-- antenna end hangs off
    |                       |
    +-----------------------+
```

### Step 5

```
  [ ] W2  male-male x3
```

Link the NodeMCU power pins to the breadboard side rails.

```
    NodeMCU 3V3  ---(orange)--->  + rail  (call this 3V3 RAIL)
    NodeMCU GND  ---(black)---->  - rail  (call this GND RAIL)
    NodeMCU VIN  ---(red)------>  spare row (call this 5V ROW)
```

> **STOP — TEST GATE 1**
> Meter each rail with the board powered over USB:
> - 3V3 RAIL to GND RAIL = **3.3 V** (+/- 0.1)
> - 5V ROW to GND RAIL = **4.5–5.0 V**
>
> Wrong voltage now means a wrong voltage into every module later. Do not continue.

---

## BAG 2 — I2C bus

### Step 6

```
  [ ] P2  ADS1115     [ ] W1  male-female x5
```

Wire the ADS1115.

```
    ADS1115 VDD   ---(orange)--->  3V3 RAIL
    ADS1115 GND   ---(black)---->  GND RAIL
    ADS1115 SCL   ---(green)---->  D1
    ADS1115 SDA   ---(blue)----->  D2
    ADS1115 ADDR  ---(black)---->  GND RAIL
```

**!** ADDR must go to ground. Floating ADDR gives an unpredictable I2C address and the
firmware will not find the chip.

### Step 7

Upload an I2C scanner sketch.

> **STOP — TEST GATE 2A**
> Serial prints a device at **0x48**.
> Nothing found? ADDR is the first suspect, then SDA/SCL swapped.

### Step 8

```
  [ ] P3  OLED     [ ] W1  male-female x4
```

Add the OLED to the **same** bus.

```
    OLED VCC  ---(orange)--->  3V3 RAIL
    OLED GND  ---(black)---->  GND RAIL
    OLED SCL  ---(green)---->  D1      <-- same pin as ADS1115
    OLED SDA  ---(blue)----->  D2      <-- same pin as ADS1115
```

**!** Read the silkscreen. Some OLED clones print SDA and SCL in the opposite order to the
ADS1115.

Add no extra resistors. Both breakouts carry their own bus pull-ups.

> **STOP — TEST GATE 2B**
> The scanner now prints **two** devices: `0x48` and `0x3C`.
> Only one appears = a wiring fault on the missing one, not a bus conflict.

---

## BAG 3 — Temperature

### Step 9

```
  [ ] P4  DS18B20     [ ] R1  4.7 kOhm
```

Push the probe's three bare leads into three separate breadboard rows.

**!** Wire colours vary by batch. Usually red = VDD, black = GND, yellow = data — but
continuity-test if you are unsure. Do not guess.

### Step 10

```
  [ ] R1  4.7 kOhm     [ ] W2  male-male x2
```

Wire the probe and fit its pull-up.

```
    DS18B20 red     ---(orange)--->  3V3 RAIL
    DS18B20 black   ---(black)---->  GND RAIL
    DS18B20 yellow  ---(yellow)--->  D6

                    3V3 RAIL
                        |
                     [R1 4.7k]
                        |
    DS18B20 yellow -----+----- D6
```

The resistor bridges the data row to the **3.3 V rail** — the same rail that powers the probe.

> **STOP — TEST GATE 3**
> A DallasTemperature example sketch reads a plausible room temperature.
> A reading of `-127` means the bus never idles high: check R1.

---

## BAG 4 — Flow

**!** This bag contains the step most likely to destroy the board. Read it fully first.

### Step 11

```
  [ ] R2  1.8 kOhm     [ ] R3  3.3 kOhm
```

Build the divider on the breadboard **before** connecting anything to it.

```
    (from flow yellow)
            |
         [R2 1.8k]
            |
            +------------> to D5
            |
         [R3 3.3k]
            |
         GND RAIL
```

### Step 12

```
  [ ] multimeter
```

Test the divider with no sensor attached. Feed 5V ROW into the top of R2.

> **STOP — TEST GATE 4A**
> The junction reads **3.2–3.3 V**.
> Higher than 3.4 V = wrong resistors. Do not connect D5.

### Step 13

```
  [ ] W2  male-male x1
```

Only now, connect the divider junction to **D5**.

### Step 14

```
  [ ] P5  YF-S201
```

Wire the flow sensor.

```
    YF-S201 red     ---(red)------>  5V ROW
    YF-S201 black   ---(black)---->  GND RAIL
    YF-S201 yellow  ---(yellow)--->  top of R2      <-- NEVER direct to D5
```

> **STOP — TEST GATE 4B**
> Blow gently through the sensor. A pulse-counting sketch increments.
> No sensor yet plumbed? A pushbutton from D5 to GND substitutes for testing the counter.

---

## BAG 5 — Alerts

### Step 15

```
  [ ] P7  buzzer     [ ] W1  male-female x3
```

Wire the buzzer.

```
    Buzzer VCC  ---(orange)--->  3V3 RAIL
    Buzzer GND  ---(black)---->  GND RAIL
    Buzzer I/O  ---(yellow)--->  D0
```

On a 2-pin module there is no I/O pin: `+` goes to D0, `-` to GND RAIL.

### Step 16

```
  [ ] P8  WS2812B     [ ] R4  330 Ohm     [ ] C1  470 uF
```

Wire the LED. Note it runs from **3.3 V**, not 5 V.

```
    WS2812B VCC  ---(orange)--->  3V3 RAIL
    WS2812B GND  ---(black)---->  GND RAIL
    WS2812B DIN  ---[R4 330]---->  D4

    C1 across the LED's own VCC and GND:

        3V3 RAIL ----+----- WS2812B VCC
                     |
                  [C1 470uF]
                     |
        GND RAIL ----+----- WS2812B GND
```

**!** C1 is polarised. The stripe marks the negative leg — it goes to GND RAIL. Backwards,
it vents.

Mount C1 physically close to the LED, not next to the NodeMCU.

**!** D4 is boot-sensitive. Wiring this last in the bag is deliberate.

> **STOP — TEST GATE 5**
> Buzzer sounds on command. LED lights and changes colour cleanly with no flicker.
> Flickering or a stuck first colour = check R4 and that the LED is on 3.3 V, not 5 V.

---

## BAG 6 — Actuation

**!** The highest-risk bag. Build it on breadboard 2, physically separate from the sensors.

### Step 17

```
  [ ] P6  relay     [ ] B1  breadboard x1     [ ] W1  male-female x3
```

Place the relay on breadboard 2. Wire the **logic side only**. No 12 V yet. No valve yet.

```
    Relay VCC  ---(orange)--->  3V3 RAIL      <-- 3.3 V, NOT 5 V
    Relay GND  ---(black)---->  GND RAIL
    Relay IN   ---(yellow)--->  D7
```

**!** If the board has a `JD-VCC` jumper, remove it and run `JD-VCC` to 5V ROW. This keeps
the logic side at 3.3 V while the coil still gets 5 V.

### Step 18

```
  [ ] multimeter
```

Measure continuity between screw terminals `COM` and `NO` while driving D7 both ways.

Record the result:

```
    D7 = LOW   ->  COM-NO is  [ ] open   [ ] closed
    D7 = HIGH  ->  COM-NO is  [ ] open   [ ] closed
```

> **STOP — TEST GATE 6A**
> You now know your board's polarity.
>
> Most modules are **active-LOW**: LOW closes the contact. The firmware ships assuming
> **active-HIGH**. If your board is active-LOW, the safety logic runs inverted — the valve
> would sit open on boot and on every reset — and it must be corrected **before** the valve
> is plumbed.
>
> The fix is one line. In `config.h`, swap these two:
>
> ```c
> #define RELAY_LEVEL_VALVE_OPEN    HIGH
> #define RELAY_LEVEL_VALVE_CLOSED  LOW
> ```
>
> to:
>
> ```c
> #define RELAY_LEVEL_VALVE_OPEN    LOW
> #define RELAY_LEVEL_VALVE_CLOSED  HIGH
> ```
>
> Never edit `actuators.cpp` for this. The drive levels are isolated in `config.h`
> specifically so a polarity flip stays a one-line change.
>
> Write your result in the build log at the end of this manual. This reading is evidence
> for the report.

### Step 19

```
  [ ] P9  solenoid     [ ] P10  12 V adapter     [ ] A1  adapter     [ ] D1  1N4007
```

Build the 12 V loop. It touches the NodeMCU **nowhere**.

```
    12V adapter (+)  ------------------>  relay COM
    relay NO         ------------------>  solenoid (+)
    solenoid (-)     ------------------>  12V adapter (-)

    D1 across the solenoid terminals:

        solenoid (+) ---+--- |<|--- +--- solenoid (-)
                             D1
                      banded end to (+)
```

**!** Use the `NO` terminal, not `NC`.
**!** Do **not** connect 12 V ground to GND RAIL. The dry contacts are the isolation.
**!** D1 banded end goes to the positive side. Backwards, it shorts the supply.

### Step 20

Power the 12 V adapter. Drive D7 to the level that closes the contact.

> **STOP — TEST GATE 6B**
> The relay clicks and the solenoid audibly actuates.
> Still no water connected at this point.

---

## BAG 7 — Firmware & network

### Step 21

```
  [ ] laptop
```

Install the ESP8266 board package and the libraries listed in `libraries.txt`.

### Step 22

```
  [ ] config.h
```

Set `SIMULATION_MODE` to `0`.

### Step 23

```
  [ ] config.h     [ ] a 2.4 GHz WPA2-Personal network
```

Fill in `WIFI_SSID` and `WIFI_PASSWORD`.

**!** The ESP8266 cannot see 5 GHz networks.
**!** It cannot join WPA2-Enterprise (networks asking for a *username* and password) or
anything behind a browser login page. Most campus WiFi is one of these. Test against the
network you will actually deploy on **before** Week 3.

**! SECURITY** — `config.h` is tracked in git. Committing real credentials publishes them
permanently; git history keeps them even after a later edit. Decide how you are handling
this before typing a real password.

### Step 24

```
  [ ] ThingSpeak account
```

Create a channel with 6 fields, then set `THINGSPEAK_CHANNEL_ID` and
`THINGSPEAK_WRITE_API_KEY` in `config.h`.

Field order is fixed by `cloud_logging.cpp`:

```
    1  pH            4  flow rate L/min
    2  corrected TDS 5  total litres
    3  temperature   6  severity
```

### Step 25

Flash the firmware. Open Serial at **115200**.

> **STOP — TEST GATE 7A**
> Serial prints a station IP. Open it in a browser on the same network — the status page loads.

### Step 26

Power the router off. Reboot the board.

> **STOP — TEST GATE 7B**
> Serial reports SoftAP fallback. Join the WiFi network `PaaniGuard-Setup`
> (password in `config.h`) and open `192.168.4.1`. The status page loads with no internet.
>
> **This is the Objective 3 demonstration.** Record it.

---

## BAG 8 — Hardening before the drift run

**!** Do not start the 7-day continuous log until this bag is complete.

### Step 27

```
  [ ] PB1  perfboard
```

Transfer the sensor side from breadboard to perfboard with soldered joints.

Dupont jumpers in breadboard holes go intermittent over days of continuous running. A
single dropout puts a gap in the dataset Objective 1 depends on, and the days cannot be
recovered inside a 4-week window.

### Step 28

Strain-relieve every cable leaving the board. Tape or hot-glue at the exit point.

### Step 29

Run for 24 hours on the bench, logging continuously.

> **STOP — TEST GATE 8**
> 24 hours of unbroken log with no resets and no gaps.
> Only then start the reference-solution drift run.

---

## BAG 9 — Analog water quality

> **Parts not yet in hand.** P11 and P12 are outstanding. Everything above is buildable
> without them.

### Step 30 — when P12 (TDS) arrives

```
  [ ] P12  TDS module
```

```
    TDS  (-)  ---(black)---->  GND RAIL
    TDS  (+)  ---(orange)--->  3V3 RAIL
    TDS  (A)  ---(yellow)--->  ADS1115 A1
```

Output is 0–2.3 V. Safe into the ADC directly. No divider needed.

### Step 31 — when P11 (pH) arrives

```
  [ ] P11  pH module     [ ] multimeter     [ ] pH 4.0 and 7.0 buffer
```

**Measure before you wire.** Power the module from 5 V, put the probe in pH 7.0 buffer,
and meter the `Po` pin against ground.

```
    expected:  about 2.5 V at pH 7.0
    range:     about 2.0-3.0 V across a normal probe
    BUT:       Po is a 5 V rail op-amp and CAN reach 5 V
```

**!** The ADS1115's absolute maximum input is VDD + 0.3 V = **3.6 V** at 3.3 V supply. A
5 V excursion damages it and back-feeds the 3.3 V rail. Gain settings do not widen this.

Fit a divider on `Po` using the same pattern as Step 11, then:

```
    pH  V+  ---(red)------>  5V ROW
    pH  G   ---(black)---->  GND RAIL
    pH  Po  ---[divider]-->  ADS1115 A0
```

Leave `Do` and `To` unconnected.

### Step 32

Record both buffer readings. They are the two-point calibration inputs — not extra work.

---

## Appendix A — Why

Explanations deliberately kept out of the steps.

**Why an ADS1115 at all.** The ESP8266 has one analog pin at 10-bit resolution. The build
needs two analog channels at useful precision. The ADS1115 gives four at 16-bit over I2C.

**Why the flow sensor needs a divider.** The YF-S201 needs 4.5 V minimum, so it cannot be
run at 3.3 V to dodge the problem. Its output swings to its supply rail. Its internal PCB
already pulls the signal up to that rail, so an external 3.3 V pull-up does **not** clamp
it — the divider is the only fix.

**Why the WS2812B runs at 3.3 V.** It wants data HIGH at 0.7x supply. From 5 V that is
3.5 V, which a 3.3 V GPIO cannot reach — causing the classic flickering-first-pixel fault.
At 3.3 V supply the threshold drops to ~2.3 V and the GPIO drives it cleanly. Note that
3.3 V is below the part's 3.5 V datasheet minimum; the datasheet-correct fix is a
74AHCT125 level shifter. The substitution should be noted in the report.

**Why the relay's logic side runs at 3.3 V.** At 5 V, `IN` becomes a 5 V node. A 3.3 V GPIO
cannot pull it high enough to switch off cleanly, and while the pin is high-impedance during
boot and reset, `IN` floats toward 4 V and back-feeds a pin that is not 5 V tolerant.

**Why a normally-closed valve on the NO contact.** Unpowered NC valve = shut. Idle relay =
contact open = solenoid unpowered = no water. Power loss therefore fails closed, which is
the correct direction for a safety device.

**Why the flyback diode.** The solenoid coil generates a reverse voltage spike when it
releases. Without D1 that spike arcs across the relay contacts and pits them over time.

**Why two breadboards.** Relay switching injects noise into analog sensor lines. Physical
separation is the cheapest mitigation. Expect to spend a session on this during integration.

---

## Appendix B — Pin map

| NodeMCU | GPIO | Connects to |
|---------|------|-------------|
| D1 | 5 | ADS1115 SCL + OLED SCL |
| D2 | 4 | ADS1115 SDA + OLED SDA |
| D5 | 14 | Flow divider output |
| D6 | 12 | DS18B20 data |
| D7 | 13 | Relay IN |
| D4 | 2 | WS2812B DIN (via R4) |
| D0 | 16 | Buzzer I/O |
| D3 | 0 | **leave empty** — boot-sensitive |
| D8 | 15 | **leave empty** — boot-sensitive |
| A0 | — | **leave empty** — ADS1115 replaces it |

---

## Appendix C — Troubleshooting

| Symptom | Check first |
|---------|-------------|
| I2C scanner finds nothing | ADDR to GND; SDA/SCL not swapped |
| Only one I2C device found | Wiring on the missing module, not a bus conflict |
| Temperature reads -127 | R1 4.7k pull-up missing or on the wrong rail |
| LED flickers or wrong colour | R4 missing; LED on 5 V instead of 3.3 V |
| Board resets when LED changes | C1 missing or mounted too far from the LED |
| Board resets during WiFi transmit | USB cable too long or too thin |
| Random resets after hours | Dupont contact gone intermittent — see BAG 8 |
| Relay clicks at the wrong times | Polarity inverted — see Test Gate 6A |
| No ThingSpeak data, page works | Channel ID or write key wrong |
| Serial prints garbage | Baud rate not set to 115200 |

---

## Build log

Record as you go. This is report evidence.

| Date | Bag | Gate passed | Notes |
|------|-----|-------------|-------|
| | | | |
| | | | |
| | | | |

**Relay polarity result (Step 18):** ______________________

**pH 7.0 buffer reading (Step 31):** ______________________

**pH 4.0 buffer reading (Step 32):** ______________________
