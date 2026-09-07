// Arduino.h — minimal host-side stand-in for the Arduino core, just enough
// to compile drift_compensation.cpp and fingerprinting.cpp natively on a
// dev machine. NOT part of the firmware; only used by host_sim's
// standalone test harness (see README.md — "Running the logic without a
// board"). The real firmware always compiles against the actual ESP8266
// Arduino core via arduino-cli.

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

#define IRAM_ATTR
#define F(x) (x)

// Not used for real elapsed-time behavior by drift_compensation.cpp /
// fingerprinting.cpp (both are driven by explicit dayIndex values in the
// test harness) — just needs to exist and return a stable value so
// dayIndexAnchorMillis bookkeeping compiles and doesn't divide oddly.
inline unsigned long millis() { return 0; }

class SerialClass {
public:
  void print(const char *s) { std::printf("%s", s); }
  void print(int v) { std::printf("%d", v); }
  void print(unsigned int v) { std::printf("%u", v); }
  void print(long v) { std::printf("%ld", v); }
  void print(unsigned long v) { std::printf("%lu", v); }
  void print(float v) { std::printf("%.2f", v); }
  void print(double v) { std::printf("%.2f", v); }
  void print(char c) { std::printf("%c", c); }
  void println() { std::printf("\n"); }
  void println(const char *s) { std::printf("%s\n", s); }
  void println(int v) { std::printf("%d\n", v); }
  void println(unsigned int v) { std::printf("%u\n", v); }
  void println(long v) { std::printf("%ld\n", v); }
  void println(unsigned long v) { std::printf("%lu\n", v); }
  void println(float v) { std::printf("%.2f\n", v); }
  void println(double v) { std::printf("%.2f\n", v); }
};

extern SerialClass Serial;
