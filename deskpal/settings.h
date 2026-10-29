#pragma once

// The settings you can change without recompiling: in the setup portal
// (see portal.h) or, before first use, in secrets.h. The portal saves them in
// the ESP32's flash (Preferences). Values from secrets.h / config.h are only
// the defaults for anything the portal has never saved.

#include <Arduino.h>

#include "config.h"

struct Settings {
  char wifiSsid[33] = WIFI_SSID;          // 32 characters is the Wi-Fi maximum
  char wifiPassword[65] = WIFI_PASSWORD;  // and 64 for a password
  char githubUser[40] = GITHUB_USER;      // GitHub logins are at most 39 characters
  char githubToken[128] = GITHUB_TOKEN;   // empty = read the public contributions page
  float latitude = WEATHER_LATITUDE;      // NAN = find the location from the IP address
  float longitude = WEATHER_LONGITUDE;
};

extern Settings settings;

// Reads saved settings over the defaults. Call once in setup().
void loadSettings();
void saveSettings();
