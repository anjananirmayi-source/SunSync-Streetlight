#include "gps_handler.h"
#include "config.h"
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

static TinyGPSPlus gps;
static HardwareSerial gpsSerial(2);
static bool lastGpsValid = false;

static bool is_leap_year(int year) {
  return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

static int days_in_month(int year, int month) {
  static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  if (month >= 1 && month <= 12) {
    return days[month - 1];
  }
  return 31;
}

void gps_init() {
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#if DEBUG
  Serial.println("[GPS] Initialized on UART2 (RX=GPIO16, TX=GPIO17, 9600 baud)");
#endif
}

void gps_update() {
  while (gpsSerial.available() > 0) {
    char c = (char)gpsSerial.read();
    gps.encode(c);
  }

  bool currentValid = gps_is_valid();
  if (currentValid != lastGpsValid) {
    lastGpsValid = currentValid;
#if DEBUG
    if (currentValid) {
      Serial.printf("[GPS] Location VALID: Lat=%.6f, Lon=%.6f\n", gps.location.lat(), gps.location.lng());
    } else {
      Serial.println("[GPS] Location INVALID or searching for satellites.");
    }
#endif
  }
}

bool gps_is_valid() {
  return gps.location.isValid() && gps.location.age() < 10000;
}

double gps_get_latitude() {
  return gps.location.lat();
}

double gps_get_longitude() {
  return gps.location.lng();
}

IstTime gps_get_ist_time() {
  IstTime ist = { 0, 0, 0, 0, 0, 0, false };
  if (!gps.time.isValid() || !gps.date.isValid() || gps.time.age() > 10000 || gps.date.age() > 10000) {
    return ist;
  }

  int y = gps.date.year();
  int m = gps.date.month();
  int d = gps.date.day();
  int h = gps.time.hour();
  int min = gps.time.minute();
  int s = gps.time.second();

  // Add IST offset: UTC + 05:30
  min += 30;
  if (min >= 60) {
    min -= 60;
    h += 1;
  }

  h += 5;
  if (h >= 24) {
    h -= 24;
    d += 1;
    int maxDays = days_in_month(y, m);
    if (d > maxDays) {
      d = 1;
      m += 1;
      if (m > 12) {
        m = 1;
        y += 1;
      }
    }
  }

  ist.year = y;
  ist.month = m;
  ist.day = d;
  ist.hour = h;
  ist.minute = min;
  ist.second = s;
  ist.valid = true;
  return ist;
}

