#pragma once

// Minimal stand-in for Arduino's Print class: Adafruit GFX inherits from it,
// and print() is how deskpal draws text.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class Print {
 public:
  virtual ~Print() = default;

  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* buffer, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buffer++);
    return n;
  }
  size_t write(const char* text) {
    return text == nullptr ? 0 : write((const uint8_t*)text, strlen(text));
  }

  size_t print(const char* text) { return write(text); }
  size_t print(char c) { return write((uint8_t)c); }
};
