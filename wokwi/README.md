# Experimental Wokwi Target

This folder is an optional ESP32 hardware experiment. It is **not** the production PaaniGuard firmware.

- Board: `wokwi-esp32-devkit-v1`
- Sketch: `wokwi.ino`
- Production board: NodeMCU ESP8266 (`esp8266:esp8266:nodemcuv2`)
- Production sketch: `../Paaniguard.ino`

Wokwi does not provide a usable ESP8266 NodeMCU board model in the installed
extension, so this target cannot validate ESP8266-specific behavior. It must
not be flashed to ESP8266 hardware.

TDS and pH remain synthetic. The flow pushbutton is only a simulation
substitute for a real flow sensor.

Build this target from the repository root:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 --build-path wokwi/build --export-binaries wokwi
```

Open this folder as the VS Code workspace, then open `diagram.json` and start
Wokwi. For the real firmware, build the repository root instead:

```powershell
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 .
```
