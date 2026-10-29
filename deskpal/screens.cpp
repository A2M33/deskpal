#include "screens.h"

#include <math.h>

#include "config.h"
#include "settings.h"

namespace {

// The built-in font is 5x7 pixels; each character takes 6x8 including spacing.
// Text size N scales that by N.
constexpr int16_t kCharW = 6;
constexpr int16_t kBodyTop = 12;  // first pixel row below the header line

int16_t textWidth(const char* text, uint8_t size) {
  // The last character's spacing column is empty, so leave it out.
  return (int16_t)strlen(text) * kCharW * size - size;
}

void printAt(Adafruit_SSD1306& d, int16_t x, int16_t y, const char* text, uint8_t size = 1) {
  d.setTextSize(size);
  d.setCursor(x, y);
  d.print(text);
}

void printCentered(Adafruit_SSD1306& d, int16_t y, const char* text, uint8_t size = 1) {
  int16_t x = (SCREEN_WIDTH - textWidth(text, size)) / 2;
  printAt(d, x < 0 ? 0 : x, y, text, size);
}

// "°C" drawn by hand: the default font has no degree sign unless you switch code pages.
void drawDegreesC(Adafruit_SSD1306& d, int16_t x, int16_t y, uint8_t size) {
  d.drawCircle(x + size, y + size, size, SSD1306_WHITE);
  printAt(d, x + 2 * size + 2, y, "C", size);
}

void formatAge(unsigned long ageMs, char* out, size_t len) {
  const unsigned long minutes = ageMs / 60000UL;
  if (minutes < 1) snprintf(out, len, "just now");
  else if (minutes < 60) snprintf(out, len, "%lu min ago", minutes);
  else if (minutes < 48 * 60) snprintf(out, len, "%lu h ago", minutes / 60);
  else snprintf(out, len, "%lu days ago", minutes / (24 * 60));
}

void formatAgeShort(unsigned long ageMs, char* out, size_t len) {
  const unsigned long minutes = ageMs / 60000UL;
  if (minutes < 1) snprintf(out, len, "now");
  else if (minutes < 60) snprintf(out, len, "%lum", minutes);
  else if (minutes < 48 * 60) snprintf(out, len, "%luh", minutes / 60);
  else snprintf(out, len, "%lud", minutes / (24 * 60));
}

void drawHeader(Adafruit_SSD1306& d, const char* title, const StatusInfo& s) {
  d.clearDisplay();
  d.setTextColor(SSD1306_WHITE);
  d.setTextWrap(false);

  // 14 characters is as wide as the title can get before it hits the icons.
  char clipped[15];
  strlcpy(clipped, title, sizeof(clipped));
  printAt(d, 0, 0, clipped);

  if (s.paused) {  // two bars, like a media player's pause button
    d.fillRect(86, 1, 2, 6, SSD1306_WHITE);
    d.fillRect(89, 1, 2, 6, SSD1306_WHITE);
  }

  if (s.wifiConnected) {  // three signal bars
    d.fillRect(95, 5, 2, 2, SSD1306_WHITE);
    d.fillRect(98, 3, 2, 4, SSD1306_WHITE);
    d.fillRect(101, 1, 2, 6, SSD1306_WHITE);
  } else {  // a small cross
    d.drawLine(96, 1, 101, 6, SSD1306_WHITE);
    d.drawLine(96, 6, 101, 1, SSD1306_WHITE);
  }

  // One dot per screen; the current one is a filled square.
  for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
    const int16_t x = 109 + i * 5;
    if (i == s.screen) d.fillRect(x, 2, 3, 3, SSD1306_WHITE);
    else d.drawPixel(x + 1, 3, SSD1306_WHITE);
  }

  d.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);
}

void drawConnecting(Adafruit_SSD1306& d) {
  printCentered(d, 22, "Connecting to Wi-Fi");
  char ssid[22];
  strlcpy(ssid, settings.wifiSsid, sizeof(ssid));
  printCentered(d, 34, ssid);
  // Dots that grow and reset, so it is obvious the device has not frozen.
  static const char* const kDots[] = {"", ".", "..", "..."};
  printCentered(d, 46, kDots[(millis() / 400) % 4]);
}

// ---------- Weather icons (each fits a 24x24 box at x, y) ----------

void drawSun(Adafruit_SSD1306& d, int16_t x, int16_t y) {
  const int16_t cx = x + 12, cy = y + 12;
  d.fillCircle(cx, cy, 5, SSD1306_WHITE);
  for (int i = 0; i < 8; i++) {  // 8 rays, 45 degrees apart
    const float a = i * PI / 4;
    d.drawLine(cx + cosf(a) * 8, cy + sinf(a) * 8, cx + cosf(a) * 11, cy + sinf(a) * 11,
               SSD1306_WHITE);
  }
}

// A cloud 14 px tall: three overlapping circles on a flat base.
void drawCloud(Adafruit_SSD1306& d, int16_t x, int16_t y) {
  d.fillCircle(x + 6, y + 9, 4, SSD1306_WHITE);
  d.fillCircle(x + 12, y + 6, 6, SSD1306_WHITE);
  d.fillCircle(x + 18, y + 9, 4, SSD1306_WHITE);
  d.fillRect(x + 6, y + 9, 12, 5, SSD1306_WHITE);
}

void drawWeatherIcon(Adafruit_SSD1306& d, int16_t x, int16_t y, WeatherIcon icon) {
  switch (icon) {
    case WeatherIcon::Sun:
      drawSun(d, x, y);
      break;
    case WeatherIcon::Cloud:
      drawCloud(d, x, y + 5);
      break;
    case WeatherIcon::Rain:
      drawCloud(d, x, y + 1);
      for (int16_t dx = 7; dx <= 17; dx += 5) {  // slanted drops
        d.drawLine(x + dx, y + 17, x + dx - 2, y + 22, SSD1306_WHITE);
      }
      break;
    case WeatherIcon::Snow:
      drawCloud(d, x, y + 1);
      for (int16_t dx = 6; dx <= 18; dx += 6) {  // little plus-shaped flakes
        const int16_t fy = y + (dx == 12 ? 21 : 19);
        d.drawFastHLine(x + dx - 1, fy, 3, SSD1306_WHITE);
        d.drawFastVLine(x + dx, fy - 1, 3, SSD1306_WHITE);
      }
      break;
  }
}

// ---------- GitHub grid ----------

// Maps a day's count to 0..3, relative to the busiest day shown.
uint8_t contributionLevel(uint8_t count, uint8_t maxCount) {
  if (count == 0) return 0;
  if (maxCount <= 1) return 3;
  const uint8_t level = (count * 3 + maxCount - 1) / maxCount;  // rounds up
  return level < 1 ? 1 : (level > 3 ? 3 : level);
}

// The OLED has no shades of green, so busier days get bigger squares instead.
void drawContributionCell(Adafruit_SSD1306& d, int16_t x, int16_t y, uint8_t level) {
  static const uint8_t kSizes[] = {1, 2, 4, 6};
  const uint8_t size = kSizes[level];
  const int16_t offset = (6 - size) / 2;
  d.fillRect(x + offset, y + offset, size, size, SSD1306_WHITE);
}

}  // namespace

void drawBootScreen(Adafruit_SSD1306& d) {
  d.clearDisplay();
  d.setTextColor(SSD1306_WHITE);
  d.setTextWrap(false);
  printCentered(d, 18, "deskpal", 2);
  printCentered(d, 44, "starting...");
}

void drawClockScreen(Adafruit_SSD1306& d, const StatusInfo& s, const tm* localTime) {
  drawHeader(d, "Clock", s);

  if (localTime == nullptr) {
    if (!s.wifiConnected) drawConnecting(d);
    else printCentered(d, 30, "Syncing time...");
    return;
  }

  char hhmm[6];
  char seconds[3];
  char date[20];
  strftime(hhmm, sizeof(hhmm), "%H:%M", localTime);
  strftime(seconds, sizeof(seconds), "%S", localTime);
  strftime(date, sizeof(date), "%a %d %b %Y", localTime);

  printAt(d, 7, 16, hhmm, 3);      // big HH:MM
  printAt(d, 99, 24, seconds, 2);  // smaller seconds, bottom-aligned with it
  printCentered(d, 52, date);
}

void drawWeatherScreen(Adafruit_SSD1306& d, const StatusInfo& s, const WeatherData& w,
                       unsigned long nowMs) {
  drawHeader(d, w.place[0] != '\0' ? w.place : "Weather", s);

  if (!w.valid) {
    if (!s.wifiConnected) {
      drawConnecting(d);
    } else if (w.lastFetchFailed) {
      printCentered(d, 24, "Weather unavailable");
      printCentered(d, 36, "retrying soon");
    } else {
      printCentered(d, 30, "Loading weather...");
    }
    return;
  }

  drawWeatherIcon(d, 2, 14, weatherIcon(w.code));

  char temp[10];
  snprintf(temp, sizeof(temp), "%.1f", w.tempC);
  printAt(d, 32, 14, temp, 2);
  drawDegreesC(d, 32 + textWidth(temp, 2) + 3, 14, 2);

  printAt(d, 32, 33, weatherText(w.code));

  char wind[20];
  snprintf(wind, sizeof(wind), "Wind %.0f km/h", w.windKmh);
  printAt(d, 32, 43, wind);

  // Always say how old the data is, and flag it when the last update failed.
  char age[16];
  char line[24];
  formatAge(nowMs - w.updatedAtMs, age, sizeof(age));
  snprintf(line, sizeof(line), w.lastFetchFailed ? "! last ok %s" : "updated %s", age);
  printAt(d, 0, 55, line);  // not 56: descenders would reach the last row, which shiftFrame() may push off
}

void drawRoomScreen(Adafruit_SSD1306& d, const StatusInfo& s, const RoomReading& r,
                    unsigned long nowMs) {
  drawHeader(d, "Room", s);

  if (!r.valid) {
    printCentered(d, 20, "No sensor reading");
    printCentered(d, 32, "Check DHT22 wiring");
    printCentered(d, 44, "(data pin GPIO15)");
    return;
  }

  char temp[10];
  char humidity[8];
  snprintf(temp, sizeof(temp), "%.1f", r.tempC);
  snprintf(humidity, sizeof(humidity), "%.0f%%", r.humidity);

  printAt(d, 4, 16, temp, 2);
  drawDegreesC(d, 4 + textWidth(temp, 2) + 3, 16, 2);
  printAt(d, 124 - textWidth(humidity, 2), 16, humidity, 2);

  printAt(d, 4, 34, "temp");
  printAt(d, 124 - textWidth("humidity", 1), 34, "humidity");

  // Comfort label in a rounded box. If the sensor stopped answering, show
  // how old the numbers are instead, so stale values are never mistaken for fresh ones.
  char label[24];
  if (r.lastReadFailed) {
    char age[16];
    formatAge(nowMs - r.readAtMs, age, sizeof(age));
    snprintf(label, sizeof(label), "! %s", age);
  } else {
    strlcpy(label, comfortLabel(r.tempC, r.humidity), sizeof(label));
  }
  const int16_t boxW = textWidth(label, 1) + 10;
  const int16_t boxX = (SCREEN_WIDTH - boxW) / 2;
  d.drawRoundRect(boxX, 46, boxW, 13, 3, SSD1306_WHITE);
  printAt(d, boxX + 5, 49, label);
}

void drawGithubScreen(Adafruit_SSD1306& d, const StatusInfo& s, const GithubData& g,
                      unsigned long nowMs) {
  char title[20];
  snprintf(title, sizeof(title), "@%s", settings.githubUser);
  drawHeader(d, title, s);

  if (!g.valid) {
    if (!s.wifiConnected) {
      drawConnecting(d);
    } else if (g.error[0] != '\0') {
      printCentered(d, 24, "GitHub error:");
      printCentered(d, 36, g.error);
    } else {
      printCentered(d, 30, "Loading GitHub...");
    }
    return;
  }

  // One column per week, one row per weekday (Sunday on top), 6 px squares on a 7 px grid.
  for (uint8_t week = 0; week < g.weekCount; week++) {
    for (uint8_t day = 0; day < 7; day++) {
      const uint8_t count = g.counts[week][day];
      if (count == GITHUB_NO_DAY) continue;
      drawContributionCell(d, week * 7, kBodyTop + day * 7, contributionLevel(count, g.maxCount));
    }
  }

  // Side panel: total for the period and the data's age. Without day counts
  // (public page in an unexpected format) there is no total to show.
  const int16_t panelX = GITHUB_WEEKS * 7 + 3;
  char total[8];
  if (g.hasCounts) snprintf(total, sizeof(total), "%u", g.total);
  else strlcpy(total, "-", sizeof(total));
  printAt(d, panelX, 14, total);
  printAt(d, panelX, 24, "total");

  char age[8];
  char line[10];
  formatAgeShort(nowMs - g.updatedAtMs, age, sizeof(age));
  snprintf(line, sizeof(line), g.lastFetchFailed ? "!%s" : "%s", age);
  printAt(d, panelX, 54, line);
}

void drawPortalScreen(Adafruit_SSD1306& d, const char* apName) {
  d.clearDisplay();
  d.setTextColor(SSD1306_WHITE);
  d.setTextWrap(false);
  printAt(d, 0, 0, "Setup");
  d.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);
  printCentered(d, 14, "Join this Wi-Fi:");
  char name[22];
  strlcpy(name, apName, sizeof(name));
  printCentered(d, 26, name);
  printCentered(d, 40, "then open");
  printCentered(d, 50, "192.168.4.1");
}

void shiftFrame(Adafruit_SSD1306& d, int8_t dx, int8_t dy) {
  if (dx == 0 && dy == 0) return;
  // Walk from the bottom-right corner, so every pixel is read before it is
  // overwritten. Rows and columns that move in from the edge stay dark.
  for (int16_t y = SCREEN_HEIGHT - 1; y >= 0; y--) {
    for (int16_t x = SCREEN_WIDTH - 1; x >= 0; x--) {
      const int16_t fromX = x - dx, fromY = y - dy;
      const bool on = fromX >= 0 && fromY >= 0 && d.getPixel(fromX, fromY);
      d.drawPixel(x, y, on ? SSD1306_WHITE : SSD1306_BLACK);
    }
  }
}

void drawToast(Adafruit_SSD1306& d, const char* text) {
  const int16_t w = textWidth(text, 1) + 12;
  const int16_t x = (SCREEN_WIDTH - w) / 2;
  d.fillRoundRect(x, 24, w, 16, 4, SSD1306_WHITE);
  d.setTextColor(SSD1306_BLACK);
  printAt(d, x + 6, 28, text);
  d.setTextColor(SSD1306_WHITE);
}
