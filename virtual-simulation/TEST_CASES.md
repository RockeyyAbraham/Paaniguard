# Virtual Simulation Test Cases

Run these in `index.html`. Results marked PASS mean the adapter exercises the
behavior represented by the current production source. Electrical behavior is
always MANUAL because a browser cannot validate hardware.

| Test | Input | Expected behavior | Result |
|---|---|---|---|
| Normal startup | Open simulator, defaults | SAFE, green LED, buzzer OFF, closed solenoid | PASS |
| Normal water | Normal scenario | pH near 7, TDS near reference, SAFE | PASS |
| Low pH | Low pH scenario | pH state crosses source thresholds | PASS |
| High pH | High pH scenario | pH state crosses source thresholds | PASS |
| High TDS | High TDS scenario | TDS warning/severe state appears | PASS |
| Zero flow | Abnormal flow scenario | Flow reaches zero; no flow fingerprint is defined in production | PASS |
| High flow | Abnormal flow scenario | Flow and pulse counters increase | PASS |
| Temperature | Move temperature control | Temperature compensation changes corrected TDS | PASS |
| Combined anomaly | Multi-parameter scenario | Severe state, red LED, buzzer ON, safety output LOW | PASS |
| Sensor drift | Sensor drift scenario | Raw/corrected values and drift estimate change | PASS |
| Recovery | Return to normal | Risk output recovers where source logic allows | PASS |
| Relay transition | Severe then normal | Production path keeps relay LOW/solenoid CLOSED | PASS |
| OLED update | Change any control | OLED preview updates source fields | PASS |
| Electrical safety | Any scenario | Must be validated on a bench, never in browser | MANUAL |

The production fingerprinting code does not define a flow anomaly rule, so
zero/high flow is displayed and counted but does not create a new risk state.
