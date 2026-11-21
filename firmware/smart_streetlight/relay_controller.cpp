#include "relay_controller.h"
#include "config.h"
#include "gps_handler.h"
#include "sun_calc.h"

void relay_init() {
  // Default D1: At boot the relay output is set LOW (OFF) before anything else
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
#if DEBUG
  Serial.printf("[RELAY] Initialized GPIO%d as OUTPUT, state set to LOW (OFF)\n", RELAY_PIN);
#endif
}

bool relay_is_on() {
  // Always read back physical state from hardware pin
  return digitalRead(RELAY_PIN) == HIGH;
}

void relay_set(bool on, const char* reason) {
  bool current = relay_is_on();
  if (current != on) {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
#if DEBUG
    Serial.printf("[RELAY] State changed: %s -> %s (Reason: %s)\n",
                  current ? "ON" : "OFF",
                  on ? "ON" : "OFF",
                  reason);
#endif
  }
}

void relay_automation_update() {
  if (!AUTOMATION_ENABLED) {
    return;
  }

  // Default D2: If GPS location or IST time is not valid, do not alter relay state
  if (!gps_is_valid()) {
    return;
  }

  IstTime ist = gps_get_ist_time();
  if (!ist.valid) {
    return;
  }

  SunTimes sun = sun_calculate(gps_get_latitude(), gps_get_longitude(), ist.year, ist.month, ist.day);
  if (!sun.valid) {
    return; // Polar day/night or invalid calculation -> do not change state
  }

  int nowMinutes = (ist.hour * 60) + ist.minute;
  bool shouldBeOn = false;

  if (sun.sunsetMinutes > sun.sunriseMinutes) {
    // Normal solar cycle: ON from sunset until sunrise
    shouldBeOn = (nowMinutes >= sun.sunsetMinutes || nowMinutes < sun.sunriseMinutes);
  } else {
    // Rare edge case near extreme latitudes
    shouldBeOn = (nowMinutes >= sun.sunsetMinutes && nowMinutes < sun.sunriseMinutes);
  }

  relay_set(shouldBeOn, "AUTOMATION_SCHEDULE");
}

