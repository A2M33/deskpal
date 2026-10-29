#pragma once

// Lets weather.cpp compile on a desktop so the renderer uses its real
// weatherText() and weatherIcon(). fetchWeather() is never called; if it were,
// begin() fails and it returns false without touching the network.

#include <Arduino.h>

#include "WiFiClientSecure.h"

#define HTTP_CODE_OK 200

class HTTPClient {
 public:
  void useHTTP10(bool) {}
  void setConnectTimeout(int32_t) {}
  void setTimeout(uint16_t) {}
  bool begin(WiFiClientSecure&, const char*) { return false; }
  int GET() { return -1; }
  void end() {}
  const char* getStream() { return ""; }
};
