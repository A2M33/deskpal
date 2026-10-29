#pragma once

// Every setting you might want to change lives in this file.
//
// Personal values (Wi-Fi password, GitHub token, your location) belong in
// secrets.h instead, which git ignores. Copy secrets.example.h to secrets.h
// and edit it. Anything defined there wins, because each default below is
// only used when the name is not defined yet.

#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// ---------- Wi-Fi ----------
// "Wokwi-GUEST" is the open network inside the Wokwi simulator, so the
// project runs there with no setup at all.
#ifndef WIFI_SSID
#define WIFI_SSID "Wokwi-GUEST"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// ---------- Time ----------
// POSIX TZ string. "<+0330>-3:30" means "UTC+3:30, no daylight saving" (Tehran).
// The sign is reversed on purpose: POSIX counts hours *west* of UTC.
#ifndef TIMEZONE
#define TIMEZONE "<+0330>-3:30"
#endif
#ifndef NTP_SERVER_1
#define NTP_SERVER_1 "pool.ntp.org"
#endif
#ifndef NTP_SERVER_2
#define NTP_SERVER_2 "time.google.com"
#endif

// ---------- Weather (Open-Meteo, no key needed) ----------
#ifndef WEATHER_LATITUDE
#define WEATHER_LATITUDE 35.69
#endif
#ifndef WEATHER_LONGITUDE
#define WEATHER_LONGITUDE 51.39
#endif
#ifndef WEATHER_PLACE
#define WEATHER_PLACE "Tehran"  // only used as the screen title
#endif

// ---------- GitHub ----------
#ifndef GITHUB_USER
#define GITHUB_USER "A2M33"
#endif
#ifndef GITHUB_TOKEN
#define GITHUB_TOKEN ""  // empty = the GitHub screen shows setup instructions
#endif

// ---------- Wiring ----------
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;
constexpr uint8_t PIN_DHT = 15;
constexpr uint8_t PIN_BUTTON = 4;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int16_t SCREEN_WIDTH = 128;
constexpr int16_t SCREEN_HEIGHT = 64;

// ---------- Timing (all in milliseconds) ----------
constexpr unsigned long SCREEN_ROTATE_MS = 8000;
constexpr unsigned long LONG_PRESS_MS = 800;
constexpr unsigned long TOAST_MS = 1200;           // "Paused" / "Auto-rotate" message
constexpr unsigned long DRAW_INTERVAL_MS = 250;    // redraw 4x per second, enough for a clock
constexpr unsigned long ROOM_READ_MS = 2500;       // the DHT22 needs at least 2 s between reads
constexpr unsigned long WIFI_RESTART_MS = 2UL * 60 * 1000;  // offline this long: start Wi-Fi over
constexpr unsigned long WEATHER_REFRESH_MS = 15UL * 60 * 1000;
constexpr unsigned long GITHUB_REFRESH_MS = 30UL * 60 * 1000;
constexpr unsigned long FETCH_RETRY_MS = 60UL * 1000;  // after a failed request, try again sooner
constexpr uint16_t HTTP_TIMEOUT_MS = 8000;
