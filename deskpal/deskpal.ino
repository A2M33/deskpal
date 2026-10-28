// deskpal: a small desk display for the ESP32.
//
// Four screens on a 128x64 OLED: clock, weather, room climate and a GitHub
// contribution grid. They rotate every 8 seconds. A short button press jumps to
// the next screen, a long press pauses or resumes the rotation.
//
// loop() never waits with delay(). Each job checks millis() to see whether it
// is its turn, does a small piece of work and returns. The one exception is a
// web request, which can take a few seconds (see refreshOnlineData()).

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
WeatherData weather;
GithubData github;

uint8_t currentScreen = SCREEN_CLOCK;
bool rotationPaused = false;
unsigned long lastScreenChangeMs = 0;
bool redrawNow = true;
unsigned long lastDrawMs = 0;

const char* toastText = nullptr;
unsigned long toastStartMs = 0;

bool wifiWasConnected = false;
unsigned long lastWifiAttemptMs = 0;
bool clockStarted = false;

unsigned long lastRoomReadMs = 0;
bool roomReadOnce = false;

// "When did we last try, and how long until the next try?" for each web source.
// Comparing elapsed time (now - last) instead of a target time keeps working
// when millis() wraps around to 0 after about 49 days.
unsigned long weatherLastTryMs = 0;
unsigned long weatherWaitMs = 0;  // 0 = fetch as soon as Wi-Fi is up
unsigned long githubLastTryMs = 0;
unsigned long githubWaitMs = 0;

// ---------- Wi-Fi and time ----------

void startWifi() {
  lastWifiAttemptMs = millis();
  WiFi.disconnect();
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
        configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
        clockStarted = true;
      }
    }
    return;
  }

  if (wifiWasConnected) {
    wifiWasConnected = false;
    Serial.println("Wi-Fi lost, will retry");
  }
  if (now - lastWifiAttemptMs >= WIFI_RETRY_MS) startWifi();
}

// Fills `out` with the local time and returns true, or returns false if NTP
// has not synced yet. (Before syncing, the ESP32 thinks it is 1970.)
bool getClock(tm& out) {
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;  // any date before Nov 2023 means "not synced"
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
  switch (button.update(now)) {
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

// Web requests block for up to a few seconds (longer if the server is slow,
// capped by HTTP_TIMEOUT_MS). To keep the button and display responsive, this
// does at most one request per pass through loop().
void refreshOnlineData(unsigned long now) {
  if (WiFi.status() != WL_CONNECTED) return;

  if (now - weatherLastTryMs >= weatherWaitMs) {
    const bool ok = fetchWeather(weather);
    weatherLastTryMs = millis();
    weatherWaitMs = ok ? WEATHER_REFRESH_MS : FETCH_RETRY_MS;
    redrawNow = true;
    return;
  }

  tm unused;
  const bool clockReady = getClock(unused);  // GitHub's date range needs the real date
  if (githubHasToken() && clockReady && now - githubLastTryMs >= githubWaitMs) {
    const bool ok = fetchGithub(github);
    githubLastTryMs = millis();
    githubWaitMs = ok ? GITHUB_REFRESH_MS : FETCH_RETRY_MS;
    redrawNow = true;
  }
}

// ---------- Drawing ----------

void drawCurrentScreen(unsigned long now) {
  if (!redrawNow && now - lastDrawMs < DRAW_INTERVAL_MS) return;
  redrawNow = false;
  lastDrawMs = now;

  const StatusInfo status = {currentScreen, WiFi.status() == WL_CONNECTED, rotationPaused};
  switch (currentScreen) {
    case SCREEN_CLOCK: {
      tm localTime;
      drawClockScreen(display, status, getClock(localTime) ? &localTime : nullptr);
      break;
    }
    case SCREEN_WEATHER:
      drawWeatherScreen(display, status, weather, now);
      break;
    case SCREEN_ROOM:
      drawRoomScreen(display, status, room, now);
      break;
    case SCREEN_GITHUB:
      drawGithubScreen(display, status, github, now);
      break;
  }

  if (toastText != nullptr) {
    if (now - toastStartMs < TOAST_MS) drawToast(display, toastText);
    else toastText = nullptr;
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

  lastScreenChangeMs = millis();
}

void loop() {
  // Each job reads millis() itself, because a web request in an earlier job
  // may have taken a few seconds.
  handleButton(millis());
  rotateScreens(millis());
  maintainWifi(millis());
  readRoomSensor(millis());
  refreshOnlineData(millis());
  drawCurrentScreen(millis());
}
