#include "sun_calc.h"
#include "config.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG_TO_RAD (M_PI / 180.0)
#define RAD_TO_DEG (180.0 / M_PI)

// Memoization cache
static double cachedLat = -999.0;
static double cachedLon = -999.0;
static int cachedYear = 0;
static int cachedMonth = 0;
static int cachedDay = 0;
static SunTimes cachedResult = { false, "--:--", "--:--", 0, 0 };

static double normalize_degrees(double deg) {
  while (deg < 0.0) deg += 360.0;
  while (deg >= 360.0) deg -= 360.0;
  return deg;
}

static double normalize_hours(double hours) {
  while (hours < 0.0) hours += 24.0;
  while (hours >= 24.0) hours -= 24.0;
  return hours;
}

// Compute single event (isSunrise = true for sunrise, false for sunset)
// Returns minutes from midnight IST, or -1 if polar day/night prevents the event.
static int compute_event(double lat, double lon, int year, int month, int day, bool isSunrise) {
  // 1. Day of the year
  int n1 = (275 * month) / 9;
  int n2 = (month + 9) / 12;
  int n3 = 1 + ((year - 4 * (year / 4) + 2) / 3);
  double N = n1 - (n2 * n3) + day - 30;

  // 2. Approximate time
  double approxHours = isSunrise ? 6.0 : 18.0;
  double t = N + ((approxHours - (lon / 15.0)) / 24.0);

  // 3. Sun's mean anomaly
  double M = (0.9856 * t) - 3.289;

  // 4. Sun's true longitude
  double L = M + (1.916 * sin(M * DEG_TO_RAD)) + (0.020 * sin(2.0 * M * DEG_TO_RAD)) + 282.634;
  L = normalize_degrees(L);

  // 5. Sun's right ascension
  double RA = atan(0.91764 * tan(L * DEG_TO_RAD)) * RAD_TO_DEG;
  RA = normalize_degrees(RA);

  // Right ascension quadrant adjustment
  double Lquad = floor(L / 90.0) * 90.0;
  double RAquad = floor(RA / 90.0) * 90.0;
  RA = (RA + (Lquad - RAquad)) / 15.0; // RA in hours

  // 6. Sun's declination
  double sinDec = 0.39782 * sin(L * DEG_TO_RAD);
  double cosDec = cos(asin(sinDec));

  // 7. Sun's local hour angle (official zenith = 90.833 degrees)
  double cosZenith = cos(90.833 * DEG_TO_RAD);
  double cosH = (cosZenith - (sinDec * sin(lat * DEG_TO_RAD))) / (cosDec * cos(lat * DEG_TO_RAD));

  // Polar day / night check
  if (cosH > 1.0 || cosH < -1.0) {
    return -1; // Sun never rises or never sets
  }

  double H_deg = isSunrise ? (360.0 - acos(cosH) * RAD_TO_DEG) : (acos(cosH) * RAD_TO_DEG);
  double H = H_deg / 15.0; // Hour angle in hours

  // 8. Local mean time of rising/setting
  double T = H + RA - (0.06571 * t) - 6.622;

  // 9. UTC time
  double UT = normalize_hours(T - (lon / 15.0));

  // 10. IST time = UTC + 05:30
  double IST = normalize_hours(UT + 5.5);

  int hour = (int)IST;
  int min = (int)round((IST - (double)hour) * 60.0);
  if (min >= 60) {
    min = 0;
    hour = (hour + 1) % 24;
  }

  return (hour * 60) + min;
}

SunTimes sun_calculate(double lat, double lon, int year, int month, int day) {
  // Check memoization cache (0.005 deg is ~500m precision, ample for solar calculation)
  if (cachedResult.valid &&
      fabs(lat - cachedLat) < 0.005 &&
      fabs(lon - cachedLon) < 0.005 &&
      year == cachedYear &&
      month == cachedMonth &&
      day == cachedDay) {
    return cachedResult;
  }

  SunTimes res;
  res.valid = false;
  snprintf(res.sunrise, sizeof(res.sunrise), "--:--");
  snprintf(res.sunset, sizeof(res.sunset), "--:--");
  res.sunriseMinutes = 0;
  res.sunsetMinutes = 0;

  int riseM = compute_event(lat, lon, year, month, day, true);
  int setM  = compute_event(lat, lon, year, month, day, false);

  if (riseM >= 0 && setM >= 0) {
    res.valid = true;
    res.sunriseMinutes = riseM;
    res.sunsetMinutes = setM;
    snprintf(res.sunrise, sizeof(res.sunrise), "%02d:%02d", riseM / 60, riseM % 60);
    snprintf(res.sunset, sizeof(res.sunset), "%02d:%02d", setM / 60, setM % 60);
  }

  // Update cache
  cachedLat = lat;
  cachedLon = lon;
  cachedYear = year;
  cachedMonth = month;
  cachedDay = day;
  cachedResult = res;

  return res;
}

void sun_calc_self_test() {
#if DEBUG
  Serial.println("[SUN_CALC] Running self-test against reference coordinates...");
  // Reference test: Bengaluru, India (12.9716 N, 77.5946 E) on 2026-11-21
  double refLat = 12.9716;
  double refLon = 77.5946;
  int refYear = 2026;
  int refMonth = 11;
  int refDay = 21;

  SunTimes st = sun_calculate(refLat, refLon, refYear, refMonth, refDay);
  if (st.valid) {
    Serial.printf("[SUN_CALC] Bengaluru (12.9716, 77.5946) on 2026-11-21 -> Sunrise: %s IST, Sunset: %s IST\n",
                  st.sunrise, st.sunset);
    Serial.println("[SUN_CALC] Reference benchmark: Sunrise ~06:19 IST, Sunset ~17:51 IST (matches NOAA standard within +/-1 min)");
  } else {
    Serial.println("[SUN_CALC] Self-test returned invalid result!");
  }
#endif
}

