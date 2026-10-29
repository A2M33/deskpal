#pragma once

#include <Arduino.h>

enum class WeatherIcon { Sun, Cloud, Rain, Snow };

// Where to get the weather for.
struct Location {
  bool known = false;
  float latitude = 0;
  float longitude = 0;
  char place[24] = "";  // city name for the screen title; may be empty
};

struct WeatherData {
  bool valid = false;            // true once at least one fetch has succeeded
  bool lastFetchFailed = false;  // the most recent fetch failed; values below are older
  char place[24] = "";           // screen title, copied from the Location
  float tempC = 0;
  float windKmh = 0;
  int code = 0;                  // WMO weather code from Open-Meteo
  // The location's current offset from UTC, daylight saving included.
  // Open-Meteo works it out from the coordinates (timezone=auto).
  bool utcOffsetKnown = false;
  int32_t utcOffsetSeconds = 0;
  unsigned long updatedAtMs = 0; // millis() of the last successful fetch
};

// Finds the rough location (city level) of this network's public IP address
// through ip-api.com. Blocks for up to a few seconds.
bool lookupLocation(Location& out);

// Downloads current conditions from Open-Meteo. Blocks for up to a few seconds.
// On failure the old values are kept and only lastFetchFailed is set.
bool fetchWeather(const Location& where, WeatherData& data);

// Short text (max 15 characters) for a WMO weather code, e.g. "Partly cloudy".
const char* weatherText(int code);

WeatherIcon weatherIcon(int code);
