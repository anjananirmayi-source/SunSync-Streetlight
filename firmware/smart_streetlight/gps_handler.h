#ifndef GPS_HANDLER_H
#define GPS_HANDLER_H

#include <Arduino.h>

struct IstTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  bool valid;
};

// Initialize GPS on UART2
void gps_init();

// Non-blocking update called on each loop turn to drain UART buffer
void gps_update();

// Location validity and readings
bool gps_is_valid();
double gps_get_latitude();
double gps_get_longitude();

// Satellite count (-1 if invalid or age > 10000ms)
int gps_get_satellites();

// Converted IST time and date (UTC + 05:30 with rollover)
IstTime gps_get_ist_time();

#endif // GPS_HANDLER_H

