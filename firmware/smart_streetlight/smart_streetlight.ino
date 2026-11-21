#include "config.h"
#include "secrets.h"
#include "gps_handler.h"
#include "sun_calc.h"
#include "relay_controller.h"
#include "web_server.h"

#include <WiFi.h>
#include <LittleFS.h>

static unsigned long lastWifiCheck = 0;
static unsigned long lastAutomationCheck = 0;

void setup() {
  // CRITICAL RULE D1: Set relay output LOW (OFF) first at boot before anything else
  relay_init();

  Serial.begin(SERIAL_BAUD);
  delay(200);

#if DEBUG
  Serial.println();
  Serial.println("==================================================");
  Serial.println("   ESP32 SMART STREETLIGHT SYSTEM (v2.0)         ");
  Serial.println("==================================================");
  Serial.printf("[CONFIG] AUTOMATION_ENABLED: %s\n", AUTOMATION_ENABLED ? "TRUE" : "FALSE");
  Serial.printf("[PINS] GPS_RX: GPIO%d, GPS_TX: GPIO%d, RELAY: GPIO%d\n",
                GPS_RX_PIN, GPS_TX_PIN, RELAY_PIN);
#endif

  // Initialize GPS UART2
  gps_init();

  // Run solar calculation reference self-test
  sun_calc_self_test();

  // Initialize LittleFS Flash Storage
  if (!LittleFS.begin(true)) {
#if DEBUG
    Serial.println("[LittleFS] Mount failed! Attempting formatting...");
#endif
  } else {
#if DEBUG
    Serial.printf("[LittleFS] Mounted successfully (Total: %u KB, Used: %u KB)\n",
                  (unsigned int)(LittleFS.totalBytes() / 1024),
                  (unsigned int)(LittleFS.usedBytes() / 1024));
#endif
  }

  // Connect to Wi-Fi in Station Mode (non-blocking begin)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#if DEBUG
  Serial.printf("[WiFi] Connecting to SSID: %s ...\n", WIFI_SSID);
#endif

  // Initialize HTTP API and static file server
  web_server_init();
}

void loop() {
  // 1. Drain incoming GPS bytes without blocking
  gps_update();

  // 2. Handle HTTP client requests
  web_server_handle_client();

  unsigned long currentMillis = millis();

  // 3. Periodic Wi-Fi health check (non-blocking)
  if (currentMillis - lastWifiCheck >= WIFI_RECONNECT_INTERVAL_MS) {
    lastWifiCheck = currentMillis;
    if (WiFi.status() == WL_CONNECTED) {
      static bool ipPrinted = false;
      if (!ipPrinted) {
        ipPrinted = true;
#if DEBUG
        Serial.printf("[WiFi] Connected! IP Address: http://%s/\n", WiFi.localIP().toString().c_str());
#endif
      }
    } else {
#if DEBUG
      Serial.println("[WiFi] Reconnecting to network...");
#endif
      WiFi.reconnect();
    }
  }

  // 4. Periodic Sunrise/Sunset Automation evaluation
  if (currentMillis - lastAutomationCheck >= AUTOMATION_INTERVAL_MS) {
    lastAutomationCheck = currentMillis;
    relay_automation_update();
  }
}

