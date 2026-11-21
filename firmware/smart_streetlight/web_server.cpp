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
  StaticJsonDocument<384> doc;
#endif

  bool validLocation = gps_is_valid();
  doc["gpsValid"] = validLocation;

  if (validLocation) {
    double lat = gps_get_latitude();
    double lon = gps_get_longitude();
    doc["latitude"] = lat;
    doc["longitude"] = lon;

    IstTime ist = gps_get_ist_time();
    if (ist.valid) {
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
  } else {
    doc["latitude"] = nullptr;
    doc["longitude"] = nullptr;
    doc["sunrise"] = nullptr;
    doc["sunset"] = nullptr;
  }

  // Strictly read back hardware relay pin state
  doc["lightOn"] = relay_is_on();
  doc["automationEnabled"] = relay_is_auto_mode();
  doc["mode"] = relay_get_mode_str();

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);

#if DEBUG
  Serial.println("[HTTP] Handled GET /api/status");
#endif
}

static void execute_light_switch(bool turnOn, const char* caller) {
  if (relay_is_auto_mode()) {
    send_json_error(409, "AUTOMATION_ENABLED", "Manual control disabled: automation is active");
    return;
  }

  relay_set(turnOn, caller);
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
  Serial.printf("[HTTP] Handled %s (Turn %s -> Current: %s)\n",
                caller, turnOn ? "ON" : "OFF", actualLightOn ? "ON" : "OFF");
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

  if (relay_is_auto_mode()) {
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
  if (strcmp(stateStr, "ON") == 0) {
    execute_light_switch(true, "POST /api/light");
  } else if (strcmp(stateStr, "OFF") == 0) {
    execute_light_switch(false, "POST /api/light");
  } else {
    send_json_error(400, "INVALID_VALUE", "state must be ON or OFF");
  }
}

static void handle_get_light_on() {
  if (server.method() == HTTP_OPTIONS) {
    handle_options();
    return;
  }
  execute_light_switch(true, "GET /api/light/on");
}

static void handle_get_light_off() {
  if (server.method() == HTTP_OPTIONS) {
    handle_options();
    return;
  }
  execute_light_switch(false, "GET /api/light/off");
}

static void apply_mode(bool isAuto) {
  relay_set_auto_mode(isAuto);

  send_cors_headers();
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument respDoc;
#else
  StaticJsonDocument<128> respDoc;
#endif
  respDoc["ok"] = true;
  respDoc["mode"] = relay_get_mode_str();
  respDoc["automationEnabled"] = relay_is_auto_mode();

  String output;
  serializeJson(respDoc, output);
  server.send(200, "application/json", output);

#if DEBUG
  Serial.printf("[HTTP] Mode updated: %s\n", relay_get_mode_str());
#endif
}

static void handle_mode() {
  if (server.method() == HTTP_OPTIONS) {
    handle_options();
    return;
  }

  if (server.method() == HTTP_GET) {
    // Return current mode
    send_cors_headers();
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument respDoc;
#else
    StaticJsonDocument<128> respDoc;
#endif
    respDoc["ok"] = true;
    respDoc["mode"] = relay_get_mode_str();
    respDoc["automationEnabled"] = relay_is_auto_mode();

    String output;
    serializeJson(respDoc, output);
    server.send(200, "application/json", output);
    return;
  }

  if (server.method() != HTTP_POST) {
    send_json_error(405, "METHOD_NOT_ALLOWED", "Method not allowed");
    return;
  }

  if (!server.hasArg("plain") || server.arg("plain").length() == 0) {
    send_json_error(400, "BAD_REQUEST", "Body missing or invalid JSON");
    return;
  }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  StaticJsonDocument<256> doc;
#endif

  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err || !doc.containsKey("mode") || !doc["mode"].is<const char*>()) {
    send_json_error(400, "BAD_REQUEST", "Field 'mode' is required as string (AUTO or MANUAL)");
    return;
  }

  const char* modeStr = doc["mode"];
  if (strcasecmp(modeStr, "AUTO") == 0) {
    apply_mode(true);
  } else if (strcasecmp(modeStr, "MANUAL") == 0) {
    apply_mode(false);
  } else {
    send_json_error(400, "INVALID_VALUE", "mode must be AUTO or MANUAL");
  }
}

static void handle_mode_auto() {
  if (server.method() == HTTP_OPTIONS) {
    handle_options();
    return;
  }
  apply_mode(true);
}

static void handle_mode_manual() {
  if (server.method() == HTTP_OPTIONS) {
    handle_options();
    return;
  }
  apply_mode(false);
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

  server.on("/api/light/on", HTTP_GET, handle_get_light_on);
  server.on("/api/light/on", HTTP_OPTIONS, handle_options);

  server.on("/api/light/off", HTTP_GET, handle_get_light_off);
  server.on("/api/light/off", HTTP_OPTIONS, handle_options);

  server.on("/api/mode", HTTP_POST, handle_mode);
  server.on("/api/mode", HTTP_GET, handle_mode);
  server.on("/api/mode", HTTP_OPTIONS, handle_options);

  server.on("/api/mode/auto", HTTP_GET, handle_mode_auto);
  server.on("/api/mode/auto", HTTP_OPTIONS, handle_options);

  server.on("/api/mode/manual", HTTP_GET, handle_mode_manual);
  server.on("/api/mode/manual", HTTP_OPTIONS, handle_options);

  server.onNotFound(handle_not_found);

  server.begin();
#if DEBUG
  Serial.println("[HTTP] WebServer started on port 80 (v2.1 API)");
#endif
}

void web_server_handle_client() {
  server.handleClient();
}
