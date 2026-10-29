// Draws each deskpal screen with the real code in deskpal/screens.cpp and
// writes the 128x64 frame buffers as PBM files for render.py to turn into PNGs.
//
// Usage: render <output dir>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "screens.h"
#include "settings.h"

// ---------- Host replacements for the few Arduino/firmware functions used ----------

static unsigned long gNowMs = 0;
unsigned long millis() { return gNowMs; }

// On the ESP32, settings.cpp defines this and fills it from flash. Here it
// keeps the defaults from config.h (GitHub user A2M33, Wokwi-GUEST).
Settings settings;

// ---------- Output ----------

static bool writePbm(Adafruit_SSD1306& d, const char* dir, const char* name) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s.pbm", dir, name);
  FILE* f = fopen(path, "w");
  if (f == nullptr) {
    fprintf(stderr, "cannot write %s\n", path);
    return false;
  }
  fprintf(f, "P1\n%d %d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
  for (int16_t y = 0; y < SCREEN_HEIGHT; y++) {
    for (int16_t x = 0; x < SCREEN_WIDTH; x++) fputc(d.getPixel(x, y) ? '1' : '0', f);
    fputc('\n', f);
  }
  fclose(f);
  printf("wrote %s\n", path);
  return true;
}

// ---------- Sample data ----------

// 13 weeks of contributions, oldest first, Sunday first in each week. The last
// week is the current one: today is Thursday, so Friday and Saturday are empty.
constexpr uint8_t X = GITHUB_NO_DAY;
static const uint8_t kSampleCounts[GITHUB_WEEKS][7] = {
    {0, 2, 4, 1, 3, 0, 0},  {1, 3, 0, 5, 2, 4, 0},  {0, 6, 3, 2, 7, 1, 0},
    {0, 1, 2, 0, 4, 3, 1},  {2, 5, 8, 4, 3, 0, 0},  {0, 0, 3, 6, 2, 1, 0},
    {0, 4, 7, 9, 5, 2, 0},  {1, 2, 0, 3, 6, 4, 2},  {0, 8, 5, 3, 11, 6, 0},
    {0, 3, 4, 2, 0, 1, 0},  {3, 6, 9, 7, 4, 2, 1},  {0, 5, 12, 8, 6, 3, 0},
    {1, 4, 6, 3, 5, X, X},
};

static void fillGithub(GithubData& g) {
  g.valid = true;
  g.lastFetchFailed = false;
  g.weekCount = GITHUB_WEEKS;
  g.maxCount = 0;
  g.total = 0;
  memcpy(g.counts, kSampleCounts, sizeof(g.counts));
  for (uint8_t w = 0; w < GITHUB_WEEKS; w++) {
    for (uint8_t d = 0; d < 7; d++) {
      const uint8_t c = g.counts[w][d];
      if (c == GITHUB_NO_DAY) continue;
      g.total += c;
      if (c > g.maxCount) g.maxCount = c;
    }
  }
}

// Day of the week, 0 = Sunday (Sakamoto's method).
static int weekday(int year, int month, int day) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year -= 1;
  return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <output dir>\n", argv[0]);
    return 2;
  }
  const char* out = argv[1];

  // The device has been up for a few hours; data ages are relative to this.
  gNowMs = 5UL * 60 * 60 * 1000;
  const unsigned long minute = 60UL * 1000;

  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT);
  StatusInfo status{SCREEN_CLOCK, /*wifiConnected=*/true, /*paused=*/false};
  bool ok = true;

  // Clock: Thursday 29 October 2026, 14:32:07.
  tm local{};
  local.tm_year = 2026 - 1900;
  local.tm_mon = 9;
  local.tm_mday = 29;
  local.tm_hour = 14;
  local.tm_min = 32;
  local.tm_sec = 7;
  local.tm_wday = weekday(2026, 10, 29);  // not mktime(): that would depend on the PC's time zone
  status.screen = SCREEN_CLOCK;
  drawClockScreen(display, status, &local);
  ok &= writePbm(display, out, "clock");

  // Weather: 18 C, partly cloudy (WMO code 2), wind 9 km/h, fetched 4 minutes ago.
  WeatherData weather;
  weather.valid = true;
  weather.tempC = 18.0f;
  weather.windKmh = 9.0f;
  weather.code = 2;
  strlcpy(weather.place, "Tehran", sizeof(weather.place));  // the city the IP lookup would find
  weather.updatedAtMs = gNowMs - 4 * minute;
  status.screen = SCREEN_WEATHER;
  drawWeatherScreen(display, status, weather, gNowMs);
  ok &= writePbm(display, out, "weather");

  // Room: 23.4 C, 41 % relative humidity, read 2 seconds ago.
  RoomReading room;
  room.valid = true;
  room.tempC = 23.4f;
  room.humidity = 41.0f;
  room.readAtMs = gNowMs - 2000;
  status.screen = SCREEN_ROOM;
  drawRoomScreen(display, status, room, gNowMs);
  ok &= writePbm(display, out, "room");

  // GitHub: the contribution grid, fetched 12 minutes ago.
  GithubData github;
  fillGithub(github);
  github.updatedAtMs = gNowMs - 12 * minute;
  status.screen = SCREEN_GITHUB;
  drawGithubScreen(display, status, github, gNowMs);
  ok &= writePbm(display, out, "github");

  // The setup portal, open after holding the button for 5 seconds.
  drawPortalScreen(display, PORTAL_AP_NAME);
  ok &= writePbm(display, out, "portal");

  // Frames for the animated GIF (render.py --gif): one pass through the
  // rotation, with the clock ticking for a few seconds. rotation.txt lists
  // each frame and how long it stays up, in milliseconds.
  char listPath[1024];
  snprintf(listPath, sizeof(listPath), "%s/rotation.txt", out);
  FILE* list = fopen(listPath, "w");
  if (list == nullptr) {
    fprintf(stderr, "cannot write %s\n", listPath);
    return 1;
  }
  int frame = 0;
  auto addFrame = [&](unsigned ms) {
    char name[32];
    snprintf(name, sizeof(name), "rotation-%02d", frame++);
    ok &= writePbm(display, out, name);
    fprintf(list, "%s %u\n", name, ms);
  };
  status.screen = SCREEN_CLOCK;
  for (int second = 7; second <= 10; second++) {
    local.tm_sec = second;
    drawClockScreen(display, status, &local);
    addFrame(1000);
  }
  status.screen = SCREEN_WEATHER;
  drawWeatherScreen(display, status, weather, gNowMs);
  addFrame(2500);
  status.screen = SCREEN_ROOM;
  drawRoomScreen(display, status, room, gNowMs);
  addFrame(2500);
  status.screen = SCREEN_GITHUB;
  drawGithubScreen(display, status, github, gNowMs);
  addFrame(2500);
  fclose(list);

  return ok ? 0 : 1;
}
