#include "relay_controller.h"
#include "config.h"
#include "gps_handler.h"
#include "sun_calc.h"

static bool s_autoMode = AUTOMATION_ENABLED;

// ---------------- RELAY INITIALIZATION ----------------
void relay_init() {
  pinMode(RELAY_PIN, OUTPUT);

  // Active-HIGH relay: LOW = OFF
  digitalWrite(RELAY_PIN, LOW);

  s_autoMode = AUTOMATION_ENABLED;

#if DEBUG
  Serial.printf("[RELAY] GPIO %d initialized | OFF | Mode: %s\n",
                RELAY_PIN,
                s_autoMode ? "AUTO" : "MANUAL");
#endif
}

// ---------------- READ RELAY STATE ----------------
bool relay_is_on() {
  return digitalRead(RELAY_PIN) == HIGH;
}

// ---------------- CONTROL RELAY ----------------
void relay_set(bool on, const char* reason) {
  bool currentState = relay_is_on();

  if (currentState == on) {
    return;
  }

  // Active-HIGH relay
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);

#if DEBUG
  Serial.printf("[RELAY] %s -> %s | Reason: %s\n",
                currentState ? "ON" : "OFF",
                on ? "ON" : "OFF",
                reason ? reason : "UNKNOWN");
#endif
}

// ---------------- MODE MANAGEMENT ----------------
bool relay_is_auto_mode() {
  return s_autoMode;
}

void relay_set_auto_mode(bool autoMode) {
  if (s_autoMode == autoMode) {
    return;
  }

  s_autoMode = autoMode;

#if DEBUG
  Serial.printf("[MODE] Switched to %s\n",
                s_autoMode ? "AUTO" : "MANUAL");
#endif

  // Immediately evaluate schedule when switching into AUTO
  if (s_autoMode) {
    relay_automation_update();
  }
}

const char* relay_get_mode_str() {
  return s_autoMode ? "AUTO" : "MANUAL";
}

// ---------------- AUTOMATIC LIGHTING ----------------
void relay_automation_update() {

  // Manual mode: automation must never interfere
  if (!s_autoMode) {
    return;
  }

  // Wait for valid GPS fix
  if (!gps_is_valid()) {
#if DEBUG
    Serial.println("[AUTO] Waiting for valid GPS location...");
#endif
    return;
  }

  // Wait for valid IST time
  IstTime ist = gps_get_ist_time();

  if (!ist.valid) {
#if DEBUG
    Serial.println("[AUTO] Waiting for valid GPS time...");
#endif
    return;
  }

  // Calculate today's sunrise and sunset
  SunTimes sun = sun_calculate(
    gps_get_latitude(),
    gps_get_longitude(),
    ist.year,
    ist.month,
    ist.day
  );

  if (!sun.valid) {
#if DEBUG
    Serial.println("[AUTO] Sunrise/sunset unavailable");
#endif
    return;
  }

  int currentMinutes = ist.hour * 60 + ist.minute;

  bool shouldBeOn;

  // Normal sunrise/sunset cycle
  if (sun.sunriseMinutes < sun.sunsetMinutes) {

    // ON from sunset until midnight,
    // and from midnight until sunrise
    shouldBeOn =
      (currentMinutes >= sun.sunsetMinutes ||
       currentMinutes < sun.sunriseMinutes);

  } else {

    // Special solar-day edge case
    shouldBeOn =
      (currentMinutes >= sun.sunsetMinutes &&
       currentMinutes < sun.sunriseMinutes);
  }

#if DEBUG
  Serial.printf("[AUTO] Time: %02d:%02d | Sunrise: %s | Sunset: %s | Target: %s\n",
                ist.hour,
                ist.minute,
                sun.sunrise,
                sun.sunset,
                shouldBeOn ? "ON" : "OFF");
#endif

  // Apply automatic decision
  relay_set(shouldBeOn, "AUTO_SUNRISE_SUNSET");
}
