#pragma once

// Just enough of the Arduino core to compile deskpal's drawing code and
// Adafruit GFX on a desktop computer. Nothing here talks to hardware.

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Print.h"

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#define radians(deg) ((deg) * PI / 180.0)
#define degrees(rad) ((rad) * 180.0 / PI)

typedef bool boolean;
typedef uint8_t byte;

// The renderer sets the clock itself (see main.cpp).
unsigned long millis();

// strlcpy is in the ESP32 libc but not in every desktop libc. A macro keeps it
// from clashing with the libraries that do have it.
inline size_t deskpal_host_strlcpy(char* dst, const char* src, size_t size) {
  const size_t len = strlen(src);
  if (size > 0) {
    const size_t n = len < size - 1 ? len : size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
  }
  return len;
}
#define strlcpy deskpal_host_strlcpy

class __FlashStringHelper;

// Adafruit GFX only needs these two members of Arduino's String.
class String {
 public:
  String(const char* s = "") : text_(s) {}
  const char* c_str() const { return text_; }
  unsigned int length() const { return (unsigned int)strlen(text_); }

 private:
  const char* text_;
};

// Log output from weather.cpp goes to stderr.
class HostSerial {
 public:
  size_t printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int n = vfprintf(stderr, format, args);
    va_end(args);
    return n < 0 ? 0 : (size_t)n;
  }
  size_t println(const char* text) { return (size_t)fprintf(stderr, "%s\n", text); }
};
inline HostSerial Serial;
