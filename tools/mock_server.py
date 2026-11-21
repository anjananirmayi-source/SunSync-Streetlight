#!/usr/bin/env python3
"""
ESP32 Smart Streetlight - Mock Server (Person A)
Python 3 standard library only.

Serves frontend/ and implements API Contract v2.0:
- GET  /api/status
- POST /api/light
- OPTIONS /api/* (CORS)

Switchable scenarios:
  1. gps_ok: GPS valid, lat/lon/sunrise/sunset valid, light OFF, automation OFF.
  2. gps_waiting: GPS invalid, lat/lon/sunrise/sunset null, light OFF, automation OFF.
  3. automation_on: GPS valid, light ON, automation ON (POST /api/light -> 409).
  4. automation_off: GPS valid, light OFF, automation OFF (POST /api/light toggles light).
  5. esp_offline: Simulates disconnected ESP32 (HTTP 500 or timeout).
  6. command_fail: Status is OK, but POST /api/light fails with HTTP 500 error.
"""

import sys
import os
import json
import argparse
from http.server import HTTPServer, SimpleHTTPRequestHandler
import urllib.parse

# Default Server Configuration
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8080

# State store for mock hardware
class MockHardwareState:
    def __init__(self, scenario="gps_ok"):
        self.set_scenario(scenario)

    def set_scenario(self, scenario):
        self.scenario = scenario
        if scenario == "gps_waiting":
            self.gps_valid = False
            self.latitude = None
            self.longitude = None
            self.sunrise = None
            self.sunset = None
            self.light_on = False
            self.automation_enabled = False
        elif scenario == "automation_on":
            self.gps_valid = True
            self.latitude = 12.9716
            self.longitude = 77.5946
            self.sunrise = "06:15"
            self.sunset = "17:55"
            self.light_on = True
            self.automation_enabled = True
        elif scenario == "command_fail":
            self.gps_valid = True
            self.latitude = 12.9716
            self.longitude = 77.5946
            self.sunrise = "06:15"
            self.sunset = "17:55"
            self.light_on = False
            self.automation_enabled = False
        elif scenario == "esp_offline":
            self.gps_valid = False
            self.latitude = None
            self.longitude = None
            self.sunrise = None
            self.sunset = None
            self.light_on = False
            self.automation_enabled = False
        else:  # "gps_ok" or "automation_off"
            self.scenario = "gps_ok"
            self.gps_valid = True
            self.latitude = 12.9716
            self.longitude = 77.5946
            self.sunrise = "06:15"
            self.sunset = "17:55"
            self.light_on = False
            self.automation_enabled = False

    def to_status_dict(self):
        return {
            "gpsValid": self.gps_valid,
            "latitude": self.latitude,
            "longitude": self.longitude,
            "sunrise": self.sunrise,
            "sunset": self.sunset,
            "lightOn": self.light_on,
            "automationEnabled": self.automation_enabled
        }

STATE = MockHardwareState("gps_ok")

class MockRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        # Enable CORS for API routes
        if self.path.startswith("/api/"):
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        # 204 No Content for CORS preflight
        if self.path.startswith("/api/"):
            self.send_response(204)
            self.end_headers()
        else:
            self.send_error(405, "Method Not Allowed")

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)

        if parsed.path == "/api/status":
            self.handle_api_status()
            return
        elif parsed.path == "/api/mock/scenario":
            self.send_json(200, {"scenario": STATE.scenario, "state": STATE.to_status_dict()})
            return

        # Serve static frontend files
        super().do_GET()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)

        if parsed.path == "/api/light":
            self.handle_api_light()
            return
        elif parsed.path == "/api/mock/scenario":
            self.handle_set_scenario()
            return
        else:
            self.send_json(404, {
                "ok": False,
                "error": "NOT_FOUND",
                "message": f"Endpoint '{parsed.path}' not found"
            })

    def handle_api_status(self):
        if STATE.scenario == "esp_offline":
            self.send_json(500, {
                "ok": False,
                "error": "INTERNAL_ERROR",
                "message": "Simulated hardware offline"
            })
            return

        self.send_json(200, STATE.to_status_dict())

    def handle_api_light(self):
        if STATE.scenario == "esp_offline":
            self.send_json(500, {
                "ok": False,
                "error": "INTERNAL_ERROR",
                "message": "Simulated hardware offline"
            })
            return

        if STATE.scenario == "command_fail":
            self.send_json(500, {
                "ok": False,
                "error": "INTERNAL_ERROR",
                "message": "Simulated relay failure on GPIO 23"
            })
            return

        # Check automation constraint
        if STATE.automation_enabled:
            self.send_json(409, {
                "ok": False,
                "error": "AUTOMATION_ENABLED",
                "message": "Manual control is disabled while automation is enabled"
            })
            return

        # Parse request body
        content_length = int(self.headers.get("Content-Length", 0))
        if content_length == 0:
            self.send_json(400, {
                "ok": False,
                "error": "BAD_REQUEST",
                "message": "Request body missing"
            })
            return

        body_raw = self.rfile.read(content_length).decode("utf-8")
        try:
            body = json.loads(body_raw)
        except Exception:
            self.send_json(400, {
                "ok": False,
                "error": "BAD_REQUEST",
                "message": "Invalid JSON in request body"
            })
            return

        state_param = body.get("state")
        if state_param not in ("ON", "OFF"):
            self.send_json(400, {
                "ok": False,
                "error": "INVALID_VALUE",
                "message": "state must be 'ON' or 'OFF'"
            })
            return

        # Update simulated hardware state
        STATE.light_on = (state_param == "ON")
        self.send_json(200, {
            "ok": True,
            "lightOn": STATE.light_on
        })

    def handle_set_scenario(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body_raw = self.rfile.read(content_length).decode("utf-8")
        try:
            body = json.loads(body_raw)
            sc = body.get("scenario", "gps_ok")
            STATE.set_scenario(sc)
            self.send_json(200, {
                "ok": True,
                "scenario": STATE.scenario,
                "state": STATE.to_status_dict()
            })
        except Exception as e:
            self.send_json(400, {"ok": False, "error": "BAD_REQUEST", "message": str(e)})

    def send_json(self, status_code, data):
        payload = json.dumps(data).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

def run_server(host, port, scenario, frontend_dir):
    STATE.set_scenario(scenario)
    os.chdir(frontend_dir)

    server_address = (host, port)
    httpd = HTTPServer(server_address, MockRequestHandler)

    print("========================================")
    print("MOCK SERVER — not the ESP32")
    print(f"Serving frontend from: {frontend_dir}")
    print(f"Listening on: http://{host}:{port}")
    print(f"Initial Scenario: {STATE.scenario}")
    print("Available Scenarios: gps_ok, gps_waiting, automation_on, automation_off, esp_offline, command_fail")
    print("========================================")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping Mock Server...")
        httpd.server_close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ESP32 Smart Streetlight Mock Server")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Host address to bind")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Port to listen on")
    parser.add_argument(
        "--scenario",
        default="gps_ok",
        choices=["gps_ok", "gps_waiting", "automation_on", "automation_off", "esp_offline", "command_fail"],
        help="Initial simulated scenario"
    )
    args = parser.parse_args()

    # Determine frontend directory relative to this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    frontend_path = os.path.abspath(os.path.join(script_dir, "..", "frontend"))

    run_server(args.host, args.port, args.scenario, frontend_path)
