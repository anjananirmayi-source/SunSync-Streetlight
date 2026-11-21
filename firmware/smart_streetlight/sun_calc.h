#ifndef SUN_CALC_H
#define SUN_CALC_H

#include <Arduino.h>

struct SunTimes {
  bool valid;
  char sunrise[6];     // "HH:MM"
  char sunset[6];      // "HH:MM"
  int sunriseMinutes;  // Minutes from midnight IST (0..1439)
  int sunsetMinutes;   // Minutes from midnight IST (0..1439)
};

// Compute sunrise and sunset in IST for the given coordinates and date
SunTimes sun_calculate(double lat, double lon, int year, int month, int day);

// Run diagnostic self-test comparing against reference location/date
void sun_calc_self_test();

#endif // SUN_CALC_H

