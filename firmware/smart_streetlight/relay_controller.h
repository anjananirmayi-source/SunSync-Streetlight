#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>

// Initialize relay pin LOW (OFF) immediately at boot
void relay_init();

// Read actual physical relay pin state (HIGH = true / ON)
bool relay_is_on();

// Apply relay state (active-HIGH) with transition logging
void relay_set(bool on, const char* reason);

// Automation mode management (runtime controllable)
bool relay_is_auto_mode();
void relay_set_auto_mode(bool autoMode);
const char* relay_get_mode_str(); // "AUTO" or "MANUAL"

// Periodic evaluation of sunrise/sunset automation rules
void relay_automation_update();

#endif // RELAY_CONTROLLER_H
