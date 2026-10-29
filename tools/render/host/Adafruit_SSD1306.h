#pragma once

// Desktop stand-in for the SSD1306 driver. The real driver is an Adafruit_GFX
// that keeps a 1-bit frame buffer and sends it over I2C on display(). Here the
// buffer is a GFXcanvas1, so every drawing call runs the same Adafruit GFX
// code as on the ESP32 and the result can be read back pixel by pixel.

#include <Adafruit_GFX.h>

#define SSD1306_BLACK 0
#define SSD1306_WHITE 1
#define SSD1306_INVERSE 2

class Adafruit_SSD1306 : public GFXcanvas1 {
 public:
  Adafruit_SSD1306(int16_t w, int16_t h) : GFXcanvas1(w, h) {}

  void clearDisplay() { fillScreen(SSD1306_BLACK); }
  void display() {}
};
