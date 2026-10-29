// deskpal: a small desk display for the ESP32.
//
// Four screens on a 128x64 OLED: clock, weather, room climate and a GitHub
// contribution grid. They rotate every 8 seconds. A short button press jumps to
// the next screen, a long press pauses or resumes the rotation.
//
// loop() never waits with delay(). Each job checks millis() to see whether it
// is its turn, does a small piece of work and returns. Web requests can take
// several seconds, so they run in their own FreeRTOS task on the other CPU
// core (see networkTask()) and never hold up the clock, button or display.

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#include "button.h"
#include "config.h"
#include "github.h"
#include "room.h"
#include "screens.h"
#include "weather.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);  // -1: no reset pin
DHT dht(PIN_DHT, DHT22);
Button button(PIN_BUTTON, LONG_PRESS_MS);

RoomReading room;

// Written by the network task, read by loop() for drawing. Always hold
// dataMutex while touching these two; copy them out and draw from the copy.
WeatherData weather;
GithubData github;
SemaphoreHandle_t dataMutex;
volatile bool onlineDataChanged = false;  // set by the network task, cleared by loop()

uint8_t currentScreen = SCREEN_CLOCK;
bool rotationPaused = false;
unsigned long lastScreenChangeMs = 0;
bool redrawNow = true;
unsigned long lastDrawMs = 0;

const char* toastText = nullptr;
unsigned long toastStartMs = 0;

enum class Brightness : uint8_t { Day, Dim, Off };
Brightness brightness = Brightness::Day;
bool nightWake = false;  // screen woken by the button during NIGHT_OFF hours
unsigned long nightWakeStartMs = 0;

bool wifiWasConnected = false;
unsigned long wifiDownSinceMs = 0;  // when the connection dropped, or the last full restart
bool clockStarted = false;
bool timezoneReady = false;    // a time zone from the weather reply is in effect
int32_t appliedUtcOffset = 0;

unsigned long lastRoomReadMs = 0;
bool roomReadOnce = false;

// ---------- Wi-Fi and time ----------

void startWifi() {
  wifiDownSinceMs = millis();
  // After a drop, the ESP32 Wi-Fi driver reconnects by itself. maintainWifi()
  // only steps in if that has not worked for WIFI_RESTART_MS.
  WiFi.setAutoReconnect(true);
  // Wokwi's virtual access point is on channel 6. Saying so up front skips a
  // scan of every channel and connects much faster. 0 means "scan all".
  const int32_t channel = strcmp(WIFI_SSID, "Wokwi-GUEST") == 0 ? 6 : 0;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, channel);
  Serial.printf("Connecting to Wi-Fi \"%s\"...\n", WIFI_SSID);
}

void maintainWifi(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      Serial.printf("Wi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
      if (!clockStarted) {
        // Start NTP only once the network exists. After this the ESP32 keeps
        // re-syncing in the background by itself, even across reconnects.
        // Without a configured TIMEZONE the clock runs on UTC until the first
        // weather reply brings the local offset (see applyTimezoneFromWeather()).
        if (timezoneConfigured()) configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
        else configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
        clockStarted = true;
      }
    }
    return;
  }

  if (wifiWasConnected) {
    wifiWasConnected = false;
    wifiDownSinceMs = now;
    Serial.println("Wi-Fi lost, reconnecting");
  }
  // Calling WiFi.disconnect() here on every drop would cancel the driver's own
  // reconnect. Only start over from scratch after a long outage.
  if (now - wifiDownSinceMs >= WIFI_RESTART_MS) {
    Serial.println("Wi-Fi still down, starting the connection over");
    WiFi.disconnect();
    startWifi();
  }
}

bool timezoneConfigured() { return TIMEZONE[0] != '\0'; }

// True once NTP has synced. (Before syncing, the ESP32 thinks it is 1970.)
bool clockSynced() {
  return time(nullptr) >= 1700000000;  // any date before Nov 2023 means "not synced"
}

// Sets the local time zone from Open-Meteo's UTC offset, unless TIMEZONE is set.
// Called whenever new weather arrives, so a daylight saving change is picked
// up within one weather refresh.
void applyTimezoneFromWeather() {
  if (timezoneConfigured()) return;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  const bool known = weather.utcOffsetKnown;
  const int32_t offset = weather.utcOffsetSeconds;
  xSemaphoreGive(dataMutex);
  if (!known || (timezoneReady && offset == appliedUtcOffset)) return;

  // POSIX TZ counts hours west of UTC, so UTC+3:30 is written "<+0330>-3:30".
  const int32_t a = offset < 0 ? -offset : offset;
  const int hours = a / 3600, minutes = (a % 3600) / 60;
  char tz[24];
  snprintf(tz, sizeof(tz), "<%c%02d%02d>%c%d:%02d", offset < 0 ? '-' : '+', hours, minutes,
           offset < 0 ? '+' : '-', hours, minutes);
  setenv("TZ", tz, 1);
  tzset();
  appliedUtcOffset = offset;
  timezoneReady = true;
  Serial.printf("Time zone from weather location: %s\n", tz);
}

// Fills `out` with the local time and returns true, or returns false while
// the clock or the time zone is not known yet.
bool getClock(tm& out) {
  if (!clockSynced() || !(timezoneReady || timezoneConfigured())) return false;
  const time_t now = time(nullptr);
  localtime_r(&now, &out);
  return true;
}

// ---------- Button and screen rotation ----------

void showScreen(uint8_t screen, unsigned long now) {
  currentScreen = screen;
  lastScreenChangeMs = now;
  redrawNow = true;
}

void showToast(const char* text, unsigned long now) {
  toastText = text;
  toastStartMs = now;
  redrawNow = true;
}

void handleButton(unsigned long now) {
  const ButtonEvent event = button.update(now);
  if (event != ButtonEvent::None && brightness == Brightness::Off) {
    // The screen is off for the night: the press only wakes it up.
    nightWake = true;
    nightWakeStartMs = now;
    redrawNow = true;
    return;
  }
  switch (event) {
    case ButtonEvent::ShortPress:
      showScreen((currentScreen + 1) % SCREEN_COUNT, now);
      break;
    case ButtonEvent::LongPress:
      rotationPaused = !rotationPaused;
      lastScreenChangeMs = now;  // a full 8 s on this screen after resuming
      showToast(rotationPaused ? "Paused" : "Auto-rotate on", now);
      break;
    case ButtonEvent::None:
      break;
  }
}

void rotateScreens(unsigned long now) {
  if (rotationPaused) return;
  if (now - lastScreenChangeMs >= SCREEN_ROTATE_MS) {
    showScreen((currentScreen + 1) % SCREEN_COUNT, now);
  }
}

// ---------- Data sources ----------

void readRoomSensor(unsigned long now) {
  if (roomReadOnce && now - lastRoomReadMs < ROOM_READ_MS) return;
  roomReadOnce = true;
  lastRoomReadMs = now;

  const float humidity = dht.readHumidity();
  const float tempC = dht.readTemperature();
  if (isnan(humidity) || isnan(tempC)) {
    // Keep the last good values; the screen will show their age.
    room.lastReadFailed = true;
    return;
  }
  room.tempC = tempC;
  room.humidity = humidity;
  room.valid = true;
  room.lastReadFailed = false;
  room.readAtMs = now;
}

// Coordinates from config win. Without them, ask ip-api.com once per boot
// (again after FETCH_RETRY_MS if that fails). A configured WEATHER_PLACE
// replaces the city name either way.
bool resolveLocation(Location& location) {
  if (!location.known) {
    if (!isnan(WEATHER_LATITUDE) && !isnan(WEATHER_LONGITUDE)) {
      location.latitude = WEATHER_LATITUDE;
      location.longitude = WEATHER_LONGITUDE;
      location.known = true;
    } else if (!lookupLocation(location)) {
      return false;
    }
  }
  if (WEATHER_PLACE[0] != '\0') strlcpy(location.place, WEATHER_PLACE, sizeof(location.place));
  return true;
}

// Web requests block for a few seconds (longer if the server is slow, capped by
// HTTP_TIMEOUT_MS), so they live in this task on core 0. Arduino's loop() runs
// on core 1 and keeps drawing while a request is in flight.
//
// The task fetches into its own copies and only takes the mutex for the quick
// copy into the shared `weather` and `github`, so loop() never waits on the network.
void networkTask(void*) {
  static Location location;
  static WeatherData myWeather;
  static GithubData myGithub;

  // "When did we last try, and how long until the next try?" for each web source.
  // Comparing elapsed time (now - last) instead of a target time keeps working
  // when millis() wraps around to 0 after about 49 days.
  unsigned long weatherLastTryMs = 0;
  unsigned long weatherWaitMs = 0;  // 0 = fetch as soon as Wi-Fi is up
  unsigned long githubLastTryMs = 0;
  unsigned long githubWaitMs = 0;

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (millis() - weatherLastTryMs >= weatherWaitMs) {
        bool ok = resolveLocation(location);
        if (ok) ok = fetchWeather(location, myWeather);
        else myWeather.lastFetchFailed = true;
        weatherLastTryMs = millis();
        weatherWaitMs = ok ? WEATHER_REFRESH_MS : FETCH_RETRY_MS;
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        weather = myWeather;
        xSemaphoreGive(dataMutex);
        onlineDataChanged = true;
      }

      // With a token, the GraphQL date range needs the real date (UTC, so no
      // time zone needed). The public page brings its own dates.
      const bool githubReady = !githubHasToken() || clockSynced();
      if (githubReady && millis() - githubLastTryMs >= githubWaitMs) {
        const bool ok = fetchGithub(myGithub);
        githubLastTryMs = millis();
        githubWaitMs = ok ? GITHUB_REFRESH_MS : FETCH_RETRY_MS;
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        github = myGithub;
        xSemaphoreGive(dataMutex);
        onlineDataChanged = true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void startNetworkTask() {
  dataMutex = xSemaphoreCreateMutex();
  // HTTPS (mbedTLS) needs a big stack; 12 KB leaves room for the JSON parsing too.
  xTaskCreatePinnedToCore(networkTask, "network", 12 * 1024, nullptr, 1, nullptr, 0);
}

// ---------- Night mode ----------

bool isNightHour(int hour) {
  if (NIGHT_START_HOUR == NIGHT_END_HOUR) return false;  // night mode disabled
  if (NIGHT_START_HOUR < NIGHT_END_HOUR) return hour >= NIGHT_START_HOUR && hour < NIGHT_END_HOUR;
  return hour >= NIGHT_START_HOUR || hour < NIGHT_END_HOUR;  // window crosses midnight
}

void setContrast(uint8_t value) {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(value);
}

void updateBrightness(unsigned long now) {
  if (nightWake && now - nightWakeStartMs >= NIGHT_WAKE_MS) nightWake = false;

  Brightness wanted = Brightness::Day;
  tm localTime;
  if (getClock(localTime) && isNightHour(localTime.tm_hour)) {
    wanted = (NIGHT_MODE == NIGHT_OFF && !nightWake) ? Brightness::Off : Brightness::Dim;
  }
  if (wanted == brightness) return;

  if (brightness == Brightness::Off) display.ssd1306_command(SSD1306_DISPLAYON);
  switch (wanted) {
    case Brightness::Day:
      setContrast(0xCF);  // what Adafruit_SSD1306::begin() sets
      break;
    case Brightness::Dim:
      setContrast(NIGHT_CONTRAST);
      break;
    case Brightness::Off:
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      break;
  }
  brightness = wanted;
  redrawNow = true;
}

// ---------- Drawing ----------

void drawCurrentScreen(unsigned long now) {
  if (!redrawNow && now - lastDrawMs < DRAW_INTERVAL_MS) return;
  if (brightness == Brightness::Off) return;  // nothing to see, skip the I2C transfer
  redrawNow = false;
  lastDrawMs = now;

  const StatusInfo status = {currentScreen, WiFi.status() == WL_CONNECTED, rotationPaused};
  switch (currentScreen) {
    case SCREEN_CLOCK: {
      tm localTime;
      drawClockScreen(display, status, getClock(localTime) ? &localTime : nullptr);
      break;
    }
    case SCREEN_WEATHER: {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      const WeatherData copy = weather;
      xSemaphoreGive(dataMutex);
      drawWeatherScreen(display, status, copy, now);
      break;
    }
    case SCREEN_ROOM:
      drawRoomScreen(display, status, room, now);
      break;
    case SCREEN_GITHUB: {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      const GithubData copy = github;
      xSemaphoreGive(dataMutex);
      drawGithubScreen(display, status, copy, now);
      break;
    }
  }

  if (toastText != nullptr) {
    if (now - toastStartMs < TOAST_MS) drawToast(display, toastText);
    else toastText = nullptr;
  }

  if (PIXEL_SHIFT) {
    // Step through four positions, one per minute: right, right+down, down, home.
    static const int8_t kShift[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const uint8_t step = (now / PIXEL_SHIFT_MS) % 4;
    shiftFrame(display, kShift[step][0], kShift[step][1]);
  }

  display.display();  // send the finished frame to the OLED in one go
}

// ---------- Arduino entry points ----------

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("deskpal starting");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  // The last `false` tells the library not to restart Wire with default pins,
  // since we already started it on the pins above.
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false)) {
    // Keep running anyway: the serial log still works and helps find the problem.
    Serial.println("OLED not found. Check wiring and the I2C address (0x3C).");
  }
  drawBootScreen(display);
  display.display();

  dht.begin();
  button.begin();

  WiFi.mode(WIFI_STA);
  startWifi();
  startNetworkTask();

  lastScreenChangeMs = millis();
}

void loop() {
  const unsigned long now = millis();
  handleButton(now);
  rotateScreens(now);
  maintainWifi(now);
  readRoomSensor(now);
  if (onlineDataChanged) {
    onlineDataChanged = false;
    applyTimezoneFromWeather();
    redrawNow = true;
  }
  updateBrightness(now);
  drawCurrentScreen(now);
}
