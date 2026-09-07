// connectivity.h — WiFi station-mode connect with a SoftAP fallback.
// Station mode is tried first; if no known network is found within
// WIFI_CONNECT_TIMEOUT_MS, the device becomes its own access point at
// SOFTAP_SSID (default IP 192.168.4.1) so webserver.cpp always has
// something to serve on, online or not.

#ifndef PAANIGUARD_CONNECTIVITY_H
#define PAANIGUARD_CONNECTIVITY_H

#include <Arduino.h>
#include <IPAddress.h>

enum class ConnectivityMode { STATION, SOFTAP };

void connectivity_init();
ConnectivityMode connectivity_getMode();

// True only when in station mode AND actually associated — this is the
// gate cloud_logging.cpp uses to decide whether to push or skip silently.
bool connectivity_isOnline();

IPAddress connectivity_getIP();

// Call periodically from loop(). Once station mode has real NTP time, this
// corrects drift_compensation.cpp's offline day counter against it. No-op
// once synced, and no-op entirely in SoftAP mode (no internet to get NTP from).
void connectivity_maybeSyncTime();

#endif // PAANIGUARD_CONNECTIVITY_H
