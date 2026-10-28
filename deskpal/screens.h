#pragma once

#include <Adafruit_SSD1306.h>
#include <time.h>

#include "github.h"
#include "room.h"
#include "weather.h"

// The order here is the order the screens rotate in.
enum Screen : uint8_t { SCREEN_CLOCK, SCREEN_WEATHER, SCREEN_ROOM, SCREEN_GITHUB, SCREEN_COUNT };

// Things every screen shows in its header bar.
struct StatusInfo {
  uint8_t screen;
  bool wifiConnected;
  bool paused;
};

// Each draw function clears the buffer and draws one full screen.
// Call display.display() afterwards to send it to the OLED.
void drawBootScreen(Adafruit_SSD1306& d);
void drawClockScreen(Adafruit_SSD1306& d, const StatusInfo& s, const tm* localTime);
void drawWeatherScreen(Adafruit_SSD1306& d, const StatusInfo& s, const WeatherData& w,
                       unsigned long nowMs);
void drawRoomScreen(Adafruit_SSD1306& d, const StatusInfo& s, const RoomReading& r,
                    unsigned long nowMs);
void drawGithubScreen(Adafruit_SSD1306& d, const StatusInfo& s, const GithubData& g,
                      unsigned long nowMs);

// Small inverted message box drawn on top of whatever screen is showing.
void drawToast(Adafruit_SSD1306& d, const char* text);
