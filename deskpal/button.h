#pragma once

#include <Arduino.h>

// ShortPress: tap. LongPress: held for longPressMs, then released.
// Hold: still held after holdMs (fires once, without waiting for release).
enum class ButtonEvent { None, ShortPress, LongPress, Hold };

// A push button wired between a GPIO pin and GND.
// The internal pull-up resistor keeps the pin HIGH, so it reads LOW while pressed.
class Button {
 public:
  Button(uint8_t pin, unsigned long longPressMs, unsigned long holdMs)
      : pin_(pin), longPressMs_(longPressMs), holdMs_(holdMs) {}

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
        holdSent_ = false;
        return ButtonEvent::None;
      }
      // Short and long press fire on release, because until then the press
      // could still turn into a Hold. A Hold never also counts as a press.
      if (holdSent_) return ButtonEvent::None;
      return now - pressStartMs_ >= longPressMs_ ? ButtonEvent::LongPress : ButtonEvent::ShortPress;
    }
    if (pressed_ && !holdSent_ && now - pressStartMs_ >= holdMs_) {
      holdSent_ = true;
      return ButtonEvent::Hold;
    }
    return ButtonEvent::None;
  }

 private:
  static constexpr unsigned long kDebounceMs = 30;

  uint8_t pin_;
  unsigned long longPressMs_;
  unsigned long holdMs_;
  bool lastReading_ = false;
  bool pressed_ = false;
  bool holdSent_ = false;
  unsigned long lastChangeMs_ = 0;
  unsigned long pressStartMs_ = 0;
};
