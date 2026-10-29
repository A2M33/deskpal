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
  // If the public page had no per-day numbers, these hold GitHub's own
  // shade levels (0-4) instead, with maxCount = 4 and hasCounts = false.
  uint8_t counts[GITHUB_WEEKS][7];
  uint8_t maxCount = 0;          // busiest day in the grid, used to scale the squares
  bool hasCounts = true;         // false: counts are levels and total is unknown
  uint16_t total = 0;
  unsigned long updatedAtMs = 0;
};

bool githubHasToken();

// Downloads the contribution calendar for the last ~12 weeks. Blocks for a few seconds.
// With a token: through the GraphQL API (needs a synced clock for the date range).
// Without one: by reading the public github.com/users/<user>/contributions page.
bool fetchGithub(GithubData& data);
