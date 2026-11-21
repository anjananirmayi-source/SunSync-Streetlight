#include "web_server.h"
#include "config.h"
#include "gps_handler.h"
#include "sun_calc.h"
#include "relay_controller.h"

#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static WebServer server(80);

static void send_cors_headers() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void send_json_error(int httpCode, const char* errorCode, const char* message) {
  send_cors_headers();
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<256> doc;
#endif

  doc["ok"] = false;
  doc["error"] = errorCode;
  doc["message"] = message;

  String output;
  serializeJson(doc, output);
  server.send(httpCode, "application/json", output);

#if DEBUG
  Serial.printf("[HTTP] Error %d: %s - %s\n", httpCode, errorCode, message);
#endif
}

static void handle_options() {
  send_cors_headers();
  server.send(204, "text/plain", "");
}

static void handle_get_status() {
  if (server.method() == HTTP_OPTIONS) {
    handle_options();
    return;
  }
  if (server.method() != HTTP_GET) {
    send_json_error(405, "METHOD_NOT_ALLOWED", "Method not allowed");
    return;
  }

  send_cors_headers();

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<512> doc;
#endif

  bool validLocation = gps_is_valid();
  doc["gpsValid"] = validLocation;

  double lat = 0.0;
  double lon = 0.0;
  if (validLocation) {
    lat = gps_get_latitude();
    lon = gps_get_longitude();
    doc["latitude"] = lat;
    doc["longitude"] = lon;
  } else {
    doc["latitude"] = nullptr;
    doc["longitude"] = nullptr;
  }

  // Current IST time (HH:MM) from GPS
  IstTime ist = gps_get_ist_time();
  char istTimeStr[8];
  if (ist.valid) {
    snprintf(istTimeStr, sizeof(istTimeStr), "%02d:%02d", ist.hour, ist.minute);
    doc["time"] = istTimeStr;
  } else {
    doc["time"] = nullptr;
  }

  // Number of satellites tracked by TinyGPSPlus
  int sats = gps_get_satellites();
  if (sats >= 0) {
    doc["satellites"] = sats;
  } else {
    doc["satellites"] = nullptr;
  }

  // Solar schedule (Sunrise / Sunset in IST)
  if (validLocation && ist.valid) {
    SunTimes sun = sun_calculate(lat, lon, ist.year, ist.month, ist.day);
    if (sun.valid) {
      doc["sunrise"] = sun.sunrise;
      doc["sunset"] = sun.sunset;
    } else {
      doc["sunrise"] = nullptr;
      doc["sunset"] = nullptr;
    }
  } else {
    doc["sunrise"] = nullptr;
    doc["sunset"] = nullptr;
  }

  // Strictly read back hardware relay pin state
  doc["lightOn"] = relay_is_on();
  doc["automationEnabled"] = AUTOMATION_ENABLED;

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);

#if DEBUG
  Serial.println("[HTTP] Handled GET /api/status");
#endif
}

static void handle_post_light() {
  if (server.method() == HTTP_OPTIONS) {
    handle_options();
    return;
  }
  if (server.method() != HTTP_POST) {
    send_json_error(405, "METHOD_NOT_ALLOWED", "Method not allowed");
    return;
  }

  // Contract: 409 AUTOMATION_ENABLED when automation is on
  if (AUTOMATION_ENABLED) {
    send_json_error(409, "AUTOMATION_ENABLED", "Manual control disabled: automation is active");
    return;
  }

  if (!server.hasArg("plain") || server.arg("plain").length() == 0) {
    send_json_error(400, "BAD_REQUEST", "Body missing or invalid JSON");
    return;
  }

  String body = server.arg("plain");
  if (body.length() > 512) {
    send_json_error(400, "BAD_REQUEST", "Request body exceeds size limit");
    return;
  }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<256> doc;
#endif

  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    send_json_error(400, "BAD_REQUEST", "Malformed JSON body");
    return;
  }

  if (!doc.containsKey("state") || !doc["state"].is<const char*>()) {
    send_json_error(400, "BAD_REQUEST", "Field 'state' is required as string");
    return;
  }

  const char* stateStr = doc["state"];
  bool turnOn = false;
  if (strcmp(stateStr, "ON") == 0) {
    turnOn = true;
  } else if (strcmp(stateStr, "OFF") == 0) {
    turnOn = false;
  } else {
    send_json_error(400, "INVALID_VALUE", "state must be ON or OFF");
    return;
  }

  // Apply state to relay
  relay_set(turnOn, "API_MANUAL_COMMAND");

  // Read back actual state directly from hardware pin
  bool actualLightOn = relay_is_on();

  send_cors_headers();
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument respDoc;
#else
  StaticJsonDocument<128> respDoc;
#endif
  respDoc["ok"] = true;
  respDoc["lightOn"] = actualLightOn;

  String output;
  serializeJson(respDoc, output);
  server.send(200, "application/json", output);

#if DEBUG
  Serial.printf("[HTTP] Handled POST /api/light (Requested: %s, Current: %s)\n",
                stateStr, actualLightOn ? "ON" : "OFF");
#endif
}

static String get_content_type(const String& path) {
  if (path.endsWith(".html") || path.endsWith(".htm")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".gif")) return "image/gif";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".ico")) return "image/x-icon";
  if (path.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

static bool handle_file_read(String path) {
  if (path.endsWith("/")) {
    path += "index.html";
  }

  String contentType = get_content_type(path);
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

static void handle_not_found() {
  if (server.method() == HTTP_OPTIONS) {
    handle_options();
    return;
  }

  // Attempt to serve static web file from LittleFS
  if (handle_file_read(server.uri())) {
    return;
  }

  if (server.uri().startsWith("/api/")) {
    send_json_error(404, "NOT_FOUND", "API endpoint not found");
  } else {
    server.send(404, "text/plain", "404 Not Found");
  }
}

void web_server_init() {
  server.on("/api/status", HTTP_GET, handle_get_status);
  server.on("/api/status", HTTP_OPTIONS, handle_options);

  server.on("/api/light", HTTP_POST, handle_post_light);
  server.on("/api/light", HTTP_OPTIONS, handle_options);

  server.onNotFound(handle_not_found);

  server.begin();
#if DEBUG
  Serial.println("[HTTP] WebServer started on port 80");
#endif
}

void web_server_handle_client() {
  server.handleClient();
}

