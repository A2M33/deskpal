#include "github.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "config.h"

namespace {

// Asks only for what we draw: per-day counts and their weekday.
const char kQuery[] =
    "query($login:String!,$from:DateTime!,$to:DateTime!){"
    "user(login:$login){contributionsCollection(from:$from,to:$to){"
    "contributionCalendar{totalContributions weeks{contributionDays{contributionCount weekday}}}"
    "}}}";

bool fail(GithubData& data, const char* reason) {
  Serial.printf("[github] %s\n", reason);
  strlcpy(data.error, reason, sizeof(data.error));
  data.lastFetchFailed = true;
  return false;
}

}  // namespace

bool githubHasToken() { return GITHUB_TOKEN[0] != '\0'; }

bool fetchGithub(GithubData& data) {
  if (!githubHasToken()) return fail(data, "No token");

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

  const int status = http.POST(body);
  if (status != HTTP_CODE_OK) {
    http.end();
    if (status == 401) return fail(data, "Token rejected");
    if (status < 0) return fail(data, "No connection");
    char reason[24];
    snprintf(reason, sizeof(reason), "HTTP %d", status);
    return fail(data, reason);
  }

  // Keep only the fields we use. Without a filter, the reply for 13 weeks is
  // small anyway, but this keeps RAM safe if someone widens the date range.
  JsonDocument filter;
  JsonObject calendarFilter =
      filter["data"]["user"]["contributionsCollection"]["contributionCalendar"].to<JsonObject>();
  calendarFilter["totalContributions"] = true;
  calendarFilter["weeks"][0]["contributionDays"][0]["contributionCount"] = true;
  calendarFilter["weeks"][0]["contributionDays"][0]["weekday"] = true;
  filter["errors"][0]["message"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) return fail(data, "Bad reply");

  if (doc["data"]["user"].isNull()) {
    // GraphQL reports problems inside a normal 200 reply.
    const char* message = doc["errors"][0]["message"] | "unknown";
    Serial.printf("[github] API error: %s\n", message);
    return fail(data, "User not found");
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
  data.total = calendar["totalContributions"] | 0;
  data.valid = true;
  data.lastFetchFailed = false;
  data.error[0] = '\0';
  data.updatedAtMs = millis();
  Serial.printf("[github] %u contributions over %u weeks\n", data.total, data.weekCount);
  return true;
}
