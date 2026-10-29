#!/usr/bin/env python3
"""Live PaaniGuard page on this computer, fed over the USB cable.

Reads the board's serial output (the LIVE_TDS lines) and serves web/index.html
at http://localhost:8000 with the same /api/status and /api/history the board
serves over its own WiFi. No WiFi change needed, so this computer keeps its
internet. The page, the OLED and the beeper all derive from the same board
reading, so they agree.

    pip install pyserial
    python3 web/serial_bridge.py                 # /dev/ttyUSB0
    python3 web/serial_bridge.py --port /dev/ttyUSB1 --demo

Only the fields the serial line carries are real (TDS, flow, pulses, verdict
level). The rest (probe voltage, litres) is derived the way the firmware does,
and device info (memory, phones connected) is not available over serial.
This is a stopgap for development; it does not exercise the board's WiFi,
captive portal or web server. Stop it before flashing: it holds the port.
"""
import argparse
import json
import os
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing: pip install pyserial")

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = os.path.join(HERE, "index.html")

# Mirrors config.h (TDS_SAFE_MAX_PPM, TDS_SEVERE_MAX_PPM, FLOW_PULSES_PER_LITER).
SAFE_MAX, SEVERE_MAX = 500, 1000
PULSES_PER_LITER = 450.0
HIST_LEN = 120
STALE_AFTER_S = 5

# Same wording as fingerprinting.cpp, by verdict level.
MESSAGES = {
    0: "Water quality within normal parameters.",
    1: "Minor deviation detected. Continuing to monitor.",
    2: ("TDS rising steadily past the safe threshold. Possible mineral, salt, or "
        "gradual contamination build-up. Consider manual testing."),
    3: ("Severe TDS spike detected, independent of pH. Possible heavy contamination "
        "event (e.g. industrial discharge, saltwater intrusion). Water supply "
        "automatically shut off as a precaution."),
}
CONTACT = "PLACEHOLDER: Contact your local water authority / municipal helpline"

LINE = re.compile(r"LIVE_TDS:\s*([\d.]+)\s*ppm.*?Pulses:\s*(\d+).*?Rate:\s*([\d.]+)"
                  r"(?:.*?Level:\s*(\d))?(?:.*?Temp:\s*([\d.]+))?")

lock = threading.Lock()
state = {"ppm": 0.0, "pulses": 0, "lpm": 0.0, "level": 0, "rx": 0.0, "demo": False,
         "temp": None}
hist = []          # oldest first, None = no data
last_hist = 0.0
started = time.time()
args = None


def volts_from_ppm(ppm):
    """Inverse of the firmware's TDS polynomial (monotonic for v >= 0)."""
    lo, hi = 0.0, 3.3
    for _ in range(20):
        v = (lo + hi) / 2
        p = (133.42 * v ** 3 - 255.86 * v ** 2 + 857.39 * v) * 0.5
        if p < ppm:
            lo = v
        else:
            hi = v
    return (lo + hi) / 2


def level_from_ppm(ppm):
    return 3 if ppm > SEVERE_MAX else 2 if ppm > SAFE_MAX else 0


def reader():
    global last_hist
    while True:
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = args.port, 115200, 1
            s.dtr = s.rts = False          # don't reset the board when we open the port
            s.open()
            print("bridge: reading %s" % args.port)
            with s:
                while True:
                    text = s.readline().decode("utf-8", "replace")
                    if "DEMO_SENSOR_DATA" in text:
                        with lock:
                            state["demo"] = True
                    m = LINE.search(text)
                    if not m:
                        continue
                    ppm, pulses, lpm = float(m[1]), int(m[2]), float(m[3])
                    level = int(m[4]) if m[4] else level_from_ppm(ppm)
                    now = time.time()
                    with lock:
                        state.update(ppm=ppm, pulses=pulses, lpm=lpm, level=level, rx=now,
                                     temp=float(m[5]) if m[5] else None)
                        if now - last_hist >= args.interval:
                            last_hist = now
                            hist.append(round(ppm))
                            del hist[:-HIST_LEN]
        except (serial.SerialException, OSError) as e:
            print("bridge: serial problem (%s), retrying in 2 s" % e)
            time.sleep(2)


def status():
    with lock:
        st = dict(state)
    if time.time() - st["rx"] > STALE_AFTER_S:
        return None
    return {
        "uptime": int(time.time() - started),
        "demo": args.demo or st["demo"],
        "verdict": {"level": st["level"], "message": MESSAGES[st["level"]]},
        "tds": {"ppm": st["ppm"], "raw": None, "mv": volts_from_ppm(st["ppm"]) * 1000, "ok": True,
                "calibrated": False, "safeMax": SAFE_MAX, "severeMax": SEVERE_MAX},
        "flow": {"lpm": st["lpm"], "total": st["pulses"] / PULSES_PER_LITER,
                 "pulses": st["pulses"], "ok": True, "seen": st["pulses"] > 0},
        "temp": {"c": st["temp"] if st["temp"] is not None else 25.0, "ok": st["temp"] is not None},
        "ph": {"ok": False},
        "net": {"ssid": "USB cable", "ip": "localhost:%d" % args.http, "clients": 0},
        "sys": {"heap": None},
        "drift": {"n": 0, "active": False, "pct": 0.0, "recal": False, "ref": 342, "limit": 15},
        "contact": CONTACT,
    }


def history():
    with lock:
        data = [None] * (HIST_LEN - len(hist)) + list(hist)
    return {"interval": args.interval, "ppm": data}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def send(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path in ("/", "/index.html"):
            with open(PAGE, "rb") as f:
                self.send(200, "text/html; charset=utf-8", f.read())
        elif path == "/api/status":
            d = status()
            if d is None:
                self.send(503, "text/plain", b"no data from board")
            else:
                self.send(200, "application/json", json.dumps(d).encode())
        elif path == "/api/history":
            self.send(200, "application/json", json.dumps(history()).encode())
        else:
            self.send(404, "text/plain", b"not found")


def main():
    global args
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--http", type=int, default=8000, help="local web port")
    ap.add_argument("--interval", type=int, default=5, help="seconds between chart samples")
    ap.add_argument("--demo", action="store_true",
                    help="label the page 'Simulated data' (board runs the demo build)")
    args = ap.parse_args()
    threading.Thread(target=reader, daemon=True).start()
    srv = ThreadingHTTPServer(("127.0.0.1", args.http), Handler)
    print("bridge: open http://localhost:%d" % args.http)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
