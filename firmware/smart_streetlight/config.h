#ifndef CONFIG_H
#define CONFIG_H

// ========================================================
// ESP32 Smart Streetlight - System Configuration (v2.0)
// ========================================================

// Automation Switch (Compile-time)
// true  = ESP32 automatically toggles relay based on GPS sunrise/sunset.
//         POST /api/light returns HTTP 409 (AUTOMATION_ENABLED).
// false = Relay is never toggled automatically; only POST /api/light controls it.
#define AUTOMATION_ENABLED true

// Debug logging to Serial (115200 baud)
#define DEBUG true

// Hardware Pin Definitions (NON-NEGOTIABLE)
#define GPS_RX_PIN 16  // ESP32 receives from NEO-6M TX
#define GPS_TX_PIN 17  // ESP32 transmits to NEO-6M RX
#define RELAY_PIN  23  // Active-HIGH Relay IN1 (HIGH = ON, LOW = OFF)

// Communication Baud Rates
#define SERIAL_BAUD 115200
#define GPS_BAUD    9600

// Scheduling Intervals (milliseconds)
#define WIFI_RECONNECT_INTERVAL_MS 10000
#define AUTOMATION_INTERVAL_MS     1000

#endif // CONFIG_H

