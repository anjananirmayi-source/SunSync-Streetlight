#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>

// Initialize relay pin LOW (OFF) immediately at boot
void relay_init();

// Read actual physical relay pin state (HIGH = true / ON)
bool relay_is_on();

// Apply relay state (active-HIGH) with transition logging
void relay_set(bool on, const char* reason);

// Periodic evaluation of sunrise/sunset automation rules
void relay_automation_update();

#endif // RELAY_CONTROLLER_H

