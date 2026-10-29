#pragma once

// Every setting you might want to change lives in this file.
//
// Personal values (Wi-Fi password, GitHub token, your location) belong in
// secrets.h instead, which git ignores. Copy secrets.example.h to secrets.h
// and edit it. Anything defined there wins, because each default below is
// only used when the name is not defined yet.
//
// Wi-Fi, GitHub user and token, and the coordinates can also be set later in
// the setup portal (hold the button 5 seconds). What the portal saves wins
// over these defaults; see settings.h.

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
// Empty: use the UTC offset Open-Meteo reports for the weather location
// (daylight saving included, checked again with every weather update).
// Or a POSIX TZ string, e.g. "<+0330>-3:30" for "UTC+3:30, no daylight saving"
// (Tehran). The sign is reversed on purpose: POSIX counts hours *west* of UTC.
#ifndef TIMEZONE
#define TIMEZONE ""
#endif
#ifndef NTP_SERVER_1
#define NTP_SERVER_1 "pool.ntp.org"
#endif
#ifndef NTP_SERVER_2
#define NTP_SERVER_2 "time.google.com"
#endif

// ---------- Weather (Open-Meteo, no key needed) ----------
// Leave the coordinates unset (NAN) and deskpal looks up the city of your
// public IP address on ip-api.com. Set both to use a fixed place instead.
#ifndef WEATHER_LATITUDE
#define WEATHER_LATITUDE NAN
#endif
#ifndef WEATHER_LONGITUDE
#define WEATHER_LONGITUDE NAN
#endif
#ifndef WEATHER_PLACE
#define WEATHER_PLACE ""  // screen title; empty = the city from the IP lookup
#endif

// ---------- GitHub ----------
#ifndef GITHUB_USER
#define GITHUB_USER "A2M33"
#endif
#ifndef GITHUB_TOKEN
#define GITHUB_TOKEN ""  // empty = read the public contributions page instead of the API
#endif

// ---------- Setup portal ----------
// Name of the open Wi-Fi network deskpal creates for setup.
#ifndef PORTAL_AP_NAME
#define PORTAL_AP_NAME "deskpal-setup"
#endif

// ---------- Night mode ----------
// Between these hours (local time, 0-23) the screen is dimmed or switched off.
// The window may cross midnight. Set both to the same hour to turn it off.
#ifndef NIGHT_START_HOUR
#define NIGHT_START_HOUR 23
#endif
#ifndef NIGHT_END_HOUR
#define NIGHT_END_HOUR 7
#endif
#define NIGHT_DIM 1  // lower the contrast
#define NIGHT_OFF 2  // switch the screen off; a button press wakes it for NIGHT_WAKE_MS
#ifndef NIGHT_MODE
#define NIGHT_MODE NIGHT_DIM
#endif
#ifndef NIGHT_CONTRAST
#define NIGHT_CONTRAST 1  // 0-255; daytime is 207, the SSD1306 library's default
#endif

// ---------- Burn-in protection ----------
// OLED pixels that stay lit for months fade unevenly (the header line, the
// clock digits). Moving the whole picture by 1 px every minute spreads the wear.
#ifndef PIXEL_SHIFT
#define PIXEL_SHIFT 1  // 0 = off
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
constexpr unsigned long PORTAL_HOLD_MS = 5000;     // hold the button this long to open the portal
constexpr unsigned long PORTAL_AFTER_MS = 30000;   // no Wi-Fi this long after boot: open the portal
constexpr unsigned long PORTAL_TIMEOUT_MS = 5UL * 60 * 1000;  // then restart if nothing was saved
constexpr unsigned long TOAST_MS = 1200;           // "Paused" / "Auto-rotate" message
constexpr unsigned long NIGHT_WAKE_MS = 30000;     // screen on after a press, with NIGHT_OFF
constexpr unsigned long PIXEL_SHIFT_MS = 60000;    // how often the picture moves by 1 px
constexpr unsigned long DRAW_INTERVAL_MS = 250;    // redraw 4x per second, enough for a clock
constexpr unsigned long ROOM_READ_MS = 2500;       // the DHT22 needs at least 2 s between reads
constexpr unsigned long WIFI_RESTART_MS = 2UL * 60 * 1000;  // offline this long: start Wi-Fi over
constexpr unsigned long WEATHER_REFRESH_MS = 15UL * 60 * 1000;
constexpr unsigned long GITHUB_REFRESH_MS = 30UL * 60 * 1000;
constexpr unsigned long FETCH_RETRY_MS = 60UL * 1000;  // after a failed request, try again sooner
constexpr uint16_t HTTP_TIMEOUT_MS = 8000;
