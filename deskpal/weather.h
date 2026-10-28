#pragma once

#include <Arduino.h>

enum class WeatherIcon { Sun, Cloud, Rain, Snow };

struct WeatherData {
  bool valid = false;            // true once at least one fetch has succeeded
  bool lastFetchFailed = false;  // the most recent fetch failed; values below are older
  float tempC = 0;
  float windKmh = 0;
  int code = 0;                  // WMO weather code from Open-Meteo
  unsigned long updatedAtMs = 0; // millis() of the last successful fetch
};

// Downloads current conditions from Open-Meteo. Blocks for up to a few seconds.
// On failure the old values are kept and only lastFetchFailed is set.
bool fetchWeather(WeatherData& data);

// Short text (max 15 characters) for a WMO weather code, e.g. "Partly cloudy".
const char* weatherText(int code);

WeatherIcon weatherIcon(int code);
