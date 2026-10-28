#pragma once

#include <Arduino.h>

// 12 full weeks plus the current, partial one.
constexpr uint8_t GITHUB_WEEKS = 13;
// Marks grid cells with no day behind them (e.g. later this week).
constexpr uint8_t GITHUB_NO_DAY = 255;

struct GithubData {
  bool valid = false;            // true once at least one fetch has succeeded
  bool lastFetchFailed = false;  // the most recent fetch failed; values below are older
  char error[24] = "";           // short reason for the last failure, shown on screen
  uint8_t weekCount = 0;
  // counts[week][weekday]: contributions per day, weekday 0 = Sunday.
  // Capped at 254 so a single byte is enough.
  uint8_t counts[GITHUB_WEEKS][7];
  uint8_t maxCount = 0;          // busiest day in the grid, used to scale the squares
  uint16_t total = 0;
  unsigned long updatedAtMs = 0;
};

bool githubHasToken();

// Downloads the contribution calendar for the last ~12 weeks through the
// GitHub GraphQL API. Needs a token and a synced clock. Blocks for a few seconds.
bool fetchGithub(GithubData& data);
