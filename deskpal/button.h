#pragma once

#include <Arduino.h>

enum class ButtonEvent { None, ShortPress, LongPress };

// A push button wired between a GPIO pin and GND.
// The internal pull-up resistor keeps the pin HIGH, so it reads LOW while pressed.
class Button {
 public:
  Button(uint8_t pin, unsigned long longPressMs) : pin_(pin), longPressMs_(longPressMs) {}

  void begin() { pinMode(pin_, INPUT_PULLUP); }

  // Call this on every loop(). Returns at most one event per call.
  ButtonEvent update(unsigned long now) {
    const bool reading = digitalRead(pin_) == LOW;

    // Metal contacts "bounce" for a few milliseconds when pressed or released.
    // Only trust a new state once it has stayed the same for kDebounceMs.
    if (reading != lastReading_) {
      lastReading_ = reading;
      lastChangeMs_ = now;
    }
    if (now - lastChangeMs_ < kDebounceMs) return ButtonEvent::None;

    if (reading != pressed_) {
      pressed_ = reading;
      if (pressed_) {
        pressStartMs_ = now;
        longPressSent_ = false;
      } else if (!longPressSent_) {
        // Short press fires on release, so a long press never also counts as a short one.
        return ButtonEvent::ShortPress;
      }
    } else if (pressed_ && !longPressSent_ && now - pressStartMs_ >= longPressMs_) {
      // Long press fires while still held, so you see the result without letting go.
      longPressSent_ = true;
      return ButtonEvent::LongPress;
    }
    return ButtonEvent::None;
  }

 private:
  static constexpr unsigned long kDebounceMs = 30;

  uint8_t pin_;
  unsigned long longPressMs_;
  bool lastReading_ = false;
  bool pressed_ = false;
  bool longPressSent_ = false;
  unsigned long lastChangeMs_ = 0;
  unsigned long pressStartMs_ = 0;
};
