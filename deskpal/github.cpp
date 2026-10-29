#include "github.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include <memory>
#include <new>

#include "config.h"

namespace {

// Asks only for what we draw: per-day counts and their weekday.
const char kQuery[] =
    "query($login:String!,$from:DateTime!,$to:DateTime!){"
    "user(login:$login){contributionsCollection(from:$from,to:$to){"
    "contributionCalendar{totalContributions weeks{contributionDays{contributionCount weekday}}}"
    "}}}";

// Short reasons for the screen (21 characters fit on a line).
const char kBadToken[] = "Bad token";
const char kNoAccess[] = "Token lacks access";
const char kRateLimited[] = "Rate limited";
const char kUserNotFound[] = "User not found";
const char kNoConnection[] = "No connection";

bool fail(GithubData& data, const char* reason) {
  Serial.printf("[github] %s\n", reason);
  strlcpy(data.error, reason, sizeof(data.error));
  data.lastFetchFailed = true;
  return false;
}

// Turns a failed HTTP status into one of the reasons above.
const char* httpErrorReason(HTTPClient& http, int status) {
  if (status < 0) return kNoConnection;  // DNS, TLS, timeout: the request never got an answer
  if (status == 401) return kBadToken;   // wrong, expired or revoked token
  if (status == 429) return kRateLimited;
  if (status == 403) {
    // GitHub uses 403 both for "slow down" and for "this token may not do that".
    // The rate limit headers tell them apart.
    if (http.header("x-ratelimit-remaining") == "0" || http.hasHeader("retry-after")) {
      return kRateLimited;
    }
    return kNoAccess;
  }
  if (status >= 500) return "GitHub server error";
  return nullptr;
}

// GraphQL reports problems inside a normal 200 reply, with a machine-readable type.
const char* graphqlErrorReason(const char* type, const char* message) {
  if (strcmp(type, "NOT_FOUND") == 0) return kUserNotFound;
  if (strcmp(type, "RATE_LIMITED") == 0) return kRateLimited;
  if (strcmp(type, "INSUFFICIENT_SCOPES") == 0 || strcmp(type, "FORBIDDEN") == 0) return kNoAccess;
  if (strstr(message, "rate limit") != nullptr) return kRateLimited;
  if (strstr(message, "Could not resolve to a User") != nullptr) return kUserNotFound;
  return "GitHub API error";
}

// ---------- Public contributions page (no token) ----------
//
// github.com/users/<user>/contributions is the HTML calendar from the profile
// page, about 220 KB. Each day is a table cell like
//   <td ... data-date="2026-10-28" id="contribution-day-component-3-52" data-level="4" ...>
// followed by its tooltip with the exact number:
//   <tool-tip ... for="contribution-day-component-3-52" ...>39 contributions on October 28th.</tool-tip>
// The parser reads the page a character at a time and keeps only one tag and
// a small table of days, so the page itself never sits in RAM.

// Days since 1970-01-01 for "YYYY-MM-DD" (Howard Hinnant's days_from_civil).
int32_t daysFromDate(const char* s) {
  int y = atoi(s);
  const int m = atoi(s + 5), d = atoi(s + 8);
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// Copies the value of name="..." from an HTML tag. Returns false if missing.
bool tagAttribute(const char* tag, const char* name, char* out, size_t len) {
  char key[24];
  snprintf(key, sizeof(key), " %s=\"", name);
  const char* start = strstr(tag, key);
  if (start == nullptr) return false;
  start += strlen(key);
  const char* end = strchr(start, '"');
  if (end == nullptr) return false;
  const size_t n = (size_t)(end - start) < len - 1 ? (size_t)(end - start) : len - 1;
  memcpy(out, start, n);
  out[n] = '\0';
  return true;
}

class CalendarParser {
 public:
  // Days are stored by day number modulo kSlots. 128 is more than the 91 days
  // we draw, and when two days share a slot the newer one wins.
  static constexpr int kSlots = 128;
  struct Day {
    int32_t day = -1;    // days since 1970, -1 = empty
    uint8_t level = 0;   // 0-4, GitHub's shade
    int16_t count = -1;  // from the tooltip, -1 = not seen
  };

  Day days[kSlots];
  int32_t lastDay = -1;  // newest date on the page: today, in GitHub's view
  bool done = false;     // the calendar table has ended; the rest can be skipped

  void feed(char c) {
    if (c == '<') {
      if (tooltipOpen_) finishTooltip();
      inTag_ = true;
      tagLen_ = 0;
      return;
    }
    if (inTag_) {
      if (c == '>') {
        tag_[tagLen_] = '\0';
        inTag_ = false;
        handleTag();
      } else if (tagLen_ < sizeof(tag_) - 1) {
        tag_[tagLen_++] = c;
      }
      return;
    }
    if (tooltipOpen_ && textLen_ < sizeof(text_) - 1) text_[textLen_++] = c;
  }

 private:
  char tag_[400];  // the longest tags we need are about 300 characters
  size_t tagLen_ = 0;
  bool inTag_ = false;
  char cellId_[48] = "";  // id of the last day cell, to match its tooltip
  int lastSlot_ = -1;
  bool tooltipOpen_ = false;
  char text_[48];
  size_t textLen_ = 0;

  void handleTag() {
    if (strncmp(tag_, "td ", 3) == 0) {
      char date[12];
      char level[4];
      if (!tagAttribute(tag_, "data-date", date, sizeof(date)) ||
          !tagAttribute(tag_, "data-level", level, sizeof(level))) {
        return;
      }
      const int32_t day = daysFromDate(date);
      if (day < 0) return;
      const int slot = day % kSlots;
      lastSlot_ = -1;
      if (days[slot].day < day) {
        days[slot] = {day, (uint8_t)constrain(atoi(level), 0, 4), -1};
        lastSlot_ = slot;
      }
      if (day > lastDay) lastDay = day;
      if (!tagAttribute(tag_, "id", cellId_, sizeof(cellId_))) cellId_[0] = '\0';
    } else if (strncmp(tag_, "tool-tip ", 9) == 0) {
      char forId[48];
      if (lastSlot_ >= 0 && cellId_[0] != '\0' && tagAttribute(tag_, "for", forId, sizeof(forId)) &&
          strcmp(forId, cellId_) == 0) {
        tooltipOpen_ = true;
        textLen_ = 0;
      }
    } else if (strcmp(tag_, "/table") == 0 && lastDay >= 0) {
      done = true;
    }
  }

  // "No contributions on ..." or "1,234 contributions on ..."
  void finishTooltip() {
    tooltipOpen_ = false;
    text_[textLen_] = '\0';
    const char* p = text_;
    while (*p == ' ' || *p == '\n') p++;
    int count = -1;
    if (strncmp(p, "No ", 3) == 0) {
      count = 0;
    } else if (isdigit((unsigned char)*p)) {
      count = 0;
      for (; isdigit((unsigned char)*p) || *p == ','; p++) {
        if (*p != ',') count = count * 10 + (*p - '0');
        if (count > 30000) count = 30000;
      }
    }
    if (count >= 0) days[lastSlot_].count = count;
  }
};

bool fetchPublicCalendar(GithubData& data) {
  char url[96];
  snprintf(url, sizeof(url), "https://github.com/users/%s/contributions", GITHUB_USER);

  WiFiClientSecure client;
  client.setInsecure();  // see weather.cpp and the README

  HTTPClient http;
  http.useHTTP10(true);  // a plain stream, no chunked encoding to undo
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) return fail(data, "Bad URL");
  http.setUserAgent("deskpal");
  const char* headersToKeep[] = {"x-ratelimit-remaining", "retry-after"};
  http.collectHeaders(headersToKeep, 2);

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    const char* reason = status == 404 ? kUserNotFound : httpErrorReason(http, status);
    http.end();
    if (reason != nullptr) return fail(data, reason);
    char code[24];
    snprintf(code, sizeof(code), "HTTP %d", status);
    return fail(data, code);
  }

  // About 1.6 KB, so it goes on the heap instead of the network task's stack.
  std::unique_ptr<CalendarParser> parser(new (std::nothrow) CalendarParser());
  if (!parser) {
    http.end();
    return fail(data, "Out of memory");
  }

  NetworkClient* stream = http.getStreamPtr();
  uint8_t chunk[256];
  unsigned long lastDataMs = millis();
  while (!parser->done && (http.connected() || stream->available() > 0)) {
    const int available = stream->available();
    if (available <= 0) {
      if (millis() - lastDataMs > HTTP_TIMEOUT_MS) break;
      delay(10);
      continue;
    }
    const int n = stream->read(chunk, available < (int)sizeof(chunk) ? available : sizeof(chunk));
    for (int i = 0; i < n; i++) parser->feed((char)chunk[i]);
    lastDataMs = millis();
  }
  http.end();

  if (parser->lastDay < 0) return fail(data, "Bad reply");

  // Same layout as the GraphQL path: columns are Sunday-to-Saturday weeks,
  // the last one is the current week and ends at the newest day on the page.
  const int32_t lastDay = parser->lastDay;
  const int lastWeekday = (lastDay + 4) % 7;  // 1970-01-01 was a Thursday; 0 = Sunday
  const int32_t firstDay = lastDay - lastWeekday - 7 * (GITHUB_WEEKS - 1);

  // Use exact numbers only if every shown day has one, so the scale is never mixed.
  bool hasCounts = true;
  for (int32_t day = firstDay; day <= lastDay; day++) {
    const CalendarParser::Day& d = parser->days[day % CalendarParser::kSlots];
    if (d.day == day && d.count < 0) hasCounts = false;
  }

  memset(data.counts, GITHUB_NO_DAY, sizeof(data.counts));
  data.maxCount = hasCounts ? 0 : 4;
  data.total = 0;
  for (uint8_t week = 0; week < GITHUB_WEEKS; week++) {
    for (uint8_t weekday = 0; weekday < 7; weekday++) {
      const int32_t day = firstDay + week * 7 + weekday;
      const CalendarParser::Day& d = parser->days[day % CalendarParser::kSlots];
      if (d.day != day) continue;  // a future day, or not on the page
      if (!hasCounts) {
        data.counts[week][weekday] = d.level;
        continue;
      }
      const uint8_t capped = d.count > 254 ? 254 : d.count;
      data.counts[week][weekday] = capped;
      data.total += d.count;
      if (capped > data.maxCount) data.maxCount = capped;
    }
  }

  data.weekCount = GITHUB_WEEKS;
  data.hasCounts = hasCounts;
  data.valid = true;
  data.lastFetchFailed = false;
  data.error[0] = '\0';
  data.updatedAtMs = millis();
  if (hasCounts) {
    Serial.printf("[github] %u contributions over %u weeks (public page)\n", data.total,
                  data.weekCount);
  } else {
    Serial.println("[github] public page had no day counts, showing levels");
  }
  return true;
}

}  // namespace

bool githubHasToken() { return GITHUB_TOKEN[0] != '\0'; }

bool fetchGithub(GithubData& data) {
  if (!githubHasToken()) return fetchPublicCalendar(data);

  // The date range is built from the real clock, so NTP must have synced.
  const time_t now = time(nullptr);
  if (now < 1700000000) return fail(data, "Clock not set");

  // Start on the Sunday 12 weeks back, so every column is a whole week
  // (GitHub's calendar weeks run Sunday to Saturday).
  struct tm utc;
  gmtime_r(&now, &utc);
  const time_t start = now - (time_t)(utc.tm_wday + 7 * 12) * 86400;
  struct tm fromTm;
  gmtime_r(&start, &fromTm);
  fromTm.tm_hour = fromTm.tm_min = fromTm.tm_sec = 0;

  char from[24];
  char to[24];
  strftime(from, sizeof(from), "%Y-%m-%dT%H:%M:%SZ", &fromTm);
  strftime(to, sizeof(to), "%Y-%m-%dT%H:%M:%SZ", &utc);

  // Let ArduinoJson build the request body so quotes and escaping are always right.
  String body;
  {
    JsonDocument request;
    request["query"] = kQuery;
    request["variables"]["login"] = GITHUB_USER;
    request["variables"]["from"] = from;
    request["variables"]["to"] = to;
    serializeJson(request, body);
  }

  WiFiClientSecure client;
  client.setInsecure();  // see weather.cpp and the README

  HTTPClient http;
  http.useHTTP10(true);  // no chunked reply, so the stream can go straight to ArduinoJson
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, "https://api.github.com/graphql")) return fail(data, "Bad URL");

  http.setUserAgent("deskpal");  // GitHub rejects requests without a User-Agent
  http.addHeader("Authorization", String("bearer ") + GITHUB_TOKEN);
  http.addHeader("Content-Type", "application/json");
  const char* headersToKeep[] = {"x-ratelimit-remaining", "retry-after"};
  http.collectHeaders(headersToKeep, 2);

  const int status = http.POST(body);
  if (status != HTTP_CODE_OK) {
    const char* reason = httpErrorReason(http, status);
    http.end();
    if (reason != nullptr) return fail(data, reason);
    char code[24];
    snprintf(code, sizeof(code), "HTTP %d", status);
    return fail(data, code);
  }

  // Keep only the fields we use. Without a filter, the reply for 13 weeks is
  // small anyway, but this keeps RAM safe if someone widens the date range.
  JsonDocument filter;
  JsonObject calendarFilter =
      filter["data"]["user"]["contributionsCollection"]["contributionCalendar"].to<JsonObject>();
  calendarFilter["totalContributions"] = true;
  calendarFilter["weeks"][0]["contributionDays"][0]["contributionCount"] = true;
  calendarFilter["weeks"][0]["contributionDays"][0]["weekday"] = true;
  filter["errors"][0]["type"] = true;
  filter["errors"][0]["message"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) return fail(data, "Bad reply");

  if (doc["data"]["user"].isNull()) {
    const char* type = doc["errors"][0]["type"] | "";
    const char* message = doc["errors"][0]["message"] | "";
    Serial.printf("[github] API error %s: %s\n", type, message);
    return fail(data, graphqlErrorReason(type, message));
  }

  JsonObject calendar = doc["data"]["user"]["contributionsCollection"]["contributionCalendar"];
  JsonArray weeks = calendar["weeks"];

  // If the API ever returns more weeks than fit, keep the most recent ones.
  const size_t weekTotal = weeks.size();
  const size_t skip = weekTotal > GITHUB_WEEKS ? weekTotal - GITHUB_WEEKS : 0;

  memset(data.counts, GITHUB_NO_DAY, sizeof(data.counts));
  data.maxCount = 0;
  uint8_t column = 0;
  size_t index = 0;
  for (JsonObject week : weeks) {
    if (index++ < skip) continue;
    for (JsonObject day : week["contributionDays"].as<JsonArray>()) {
      const int weekday = day["weekday"] | -1;
      if (weekday < 0 || weekday > 6) continue;
      const int count = day["contributionCount"] | 0;
      const uint8_t capped = count > 254 ? 254 : count;
      data.counts[column][weekday] = capped;
      if (capped > data.maxCount) data.maxCount = capped;
    }
    column++;
  }

  data.weekCount = column;
  data.hasCounts = true;
  data.total = calendar["totalContributions"] | 0;
  data.valid = true;
  data.lastFetchFailed = false;
  data.error[0] = '\0';
  data.updatedAtMs = millis();
  Serial.printf("[github] %u contributions over %u weeks\n", data.total, data.weekCount);
  return true;
}
