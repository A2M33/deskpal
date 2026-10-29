#pragma once

// Copy this file to secrets.h (same folder) and fill in your own values.
// secrets.h is listed in .gitignore, so your password and token stay on your computer.
//
// Every line is optional. Delete or comment out a line to use the default
// from config.h instead.
//
// You can skip this file entirely: the setup portal (hold the button for
// 5 seconds) sets Wi-Fi, GitHub and location on the device. Values saved
// there win over the ones here.

// ---------- Wi-Fi ----------
// Your network name and password. Default: the open "Wokwi-GUEST" network
// that only exists inside the Wokwi simulator.
#define WIFI_SSID "my-home-wifi"
#define WIFI_PASSWORD "my-wifi-password"

// ---------- GitHub ----------
// Whose contribution grid to show. Default: "A2M33".
#define GITHUB_USER "your-github-username"

// Personal access token, optional (the README explains how to make one).
// Without it, deskpal reads your public contributions page instead. A
// fine-grained token with read-only access to public repositories is enough.
// Default: empty.
// #define GITHUB_TOKEN "github_pat_xxxxxxxxxxxxxxxxxxxxxx"

// ---------- Weather location ----------
// Find your coordinates on a map; decimal degrees, north and east positive.
// Default: unset, so deskpal looks up the city of your IP address on ip-api.com.
// #define WEATHER_LATITUDE 35.69
// #define WEATHER_LONGITUDE 51.39
// #define WEATHER_PLACE "Tehran"      // title on the weather screen, 14 characters max

// ---------- Time zone ----------
// A POSIX TZ string. Default: unset, so the clock uses the UTC offset that
// Open-Meteo reports for the weather location (daylight saving included).
// Note the reversed sign: POSIX counts hours west of UTC. Some examples:
//   Berlin:   "CET-1CEST,M3.5.0,M10.5.0/3"
//   London:   "GMT0BST,M3.5.0/1,M10.5.0"
//   New York: "EST5EDT,M3.2.0,M11.1.0"
//   UTC:      "UTC0"
// #define TIMEZONE "<+0330>-3:30"

// ---------- Display ----------
// #define CLOCK_12H 1        // "2:32 PM" instead of "14:32"
// #define TEMP_FAHRENHEIT 1  // degrees Fahrenheit instead of Celsius
// #define NIGHT_START_HOUR 23
// #define NIGHT_END_HOUR 7
// #define NIGHT_MODE NIGHT_OFF  // screen off at night instead of dimmed

// ---------- NTP servers ----------
// Where the clock gets its time. The defaults are fine almost everywhere.
// #define NTP_SERVER_1 "pool.ntp.org"
// #define NTP_SERVER_2 "time.google.com"
