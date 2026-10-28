#pragma once

// Latest reading from the DHT22 on the desk.
struct RoomReading {
  bool valid = false;           // true once at least one read has succeeded
  bool lastReadFailed = false;  // the most recent read failed; values below are older
  float tempC = 0;
  float humidity = 0;           // relative humidity in %
  unsigned long readAtMs = 0;   // millis() of the last successful read
};

// Rough indoor comfort ranges. Good enough for a desk gadget, not a lab instrument.
inline const char* comfortLabel(float tempC, float humidity) {
  if (tempC < 18) return "Cold";
  if (tempC > 28) return "Hot";
  if (humidity > 65) return "Humid";
  if (humidity < 30) return "Dry";
  if (tempC < 20) return "Cool";
  if (tempC > 25.5f) return "Warm";
  return "Comfortable";
}
