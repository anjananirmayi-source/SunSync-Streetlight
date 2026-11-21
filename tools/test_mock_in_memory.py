"""
Comprehensive in-memory HTTP test suite for MockRequestHandler.
Tests the exact HTTP request/response wire format without binding a TCP socket.
"""
import io
import json
from http.server import HTTPServer
from mock_server import MockRequestHandler, MockHardwareState, STATE

class DummyServer(HTTPServer):
    def __init__(self):
        # Do not bind socket
        self.server_name = "localhost"
        self.server_port = 8080

class TestableMockRequestHandler(MockRequestHandler):
    def __init__(self, request_bytes):
        self.rfile = io.BytesIO(request_bytes)
        self.wfile = io.BytesIO()
        self.server = DummyServer()
        self.client_address = ("127.0.0.1", 12345)
        self.close_connection = True
        self.requestline = ""
        self.command = ""
        self.path = ""
        self.request_version = "HTTP/1.1"
        self.headers = None
        self.handle_one_request()

    def get_response(self):
        self.wfile.seek(0)
        raw = self.wfile.read()
        parts = raw.split(b"\r\n\r\n", 1)
        header_lines = parts[0].decode("utf-8").split("\r\n")
        status_line = header_lines[0]
        status_code = int(status_line.split(" ")[1])
        headers = {}
        for line in header_lines[1:]:
            if ": " in line:
                k, v = line.split(": ", 1)
                headers[k.lower()] = v
        body = parts[1].decode("utf-8") if len(parts) > 1 else ""
        body_json = None
        if "application/json" in headers.get("content-type", ""):
            try:
                body_json = json.loads(body)
            except Exception:
                pass
        return status_code, headers, body_json, body

def run_tests():
    passed = 0
    total = 0

    def assert_eq(actual, expected, test_name):
        nonlocal passed, total
        total += 1
        if actual == expected:
            print(f"  [PASS] {test_name}")
            passed += 1
        else:
            print(f"  [FAIL] {test_name}: expected {expected}, got {actual}")

    print("--- 1. Testing CORS Preflight (OPTIONS) ---")
    req = b"OPTIONS /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n"
    code, hdrs, _, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 204, "OPTIONS status code is 204")
    assert_eq(hdrs.get("access-control-allow-origin"), "*", "CORS header present")

    print("\n--- 2. Testing GET /api/status (Scenario: gps_ok) ---")
    STATE.set_scenario("gps_ok")
    req = b"GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n"
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 200, "GET /api/status -> 200")
    assert_eq(data["gpsValid"], True, "gpsValid is True")
    assert_eq(data["latitude"], 12.9716, "latitude matches")
    assert_eq(data["sunrise"], "06:15", "sunrise matches")
    assert_eq(data["lightOn"], False, "lightOn is False")
    assert_eq(data["automationEnabled"], False, "automationEnabled is False")

    print("\n--- 3. Testing POST /api/light (Turn ON) ---")
    body = b'{"state":"ON"}'
    req = f"POST /api/light HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 200, "POST /api/light ON -> 200")
    assert_eq(data["ok"], True, "response ok: true")
    assert_eq(data["lightOn"], True, "lightOn: true")

    print("\n--- 4. Testing Readback of Relay State ---")
    req = b"GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n"
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(data["lightOn"], True, "Status confirms light is now ON")

    print("\n--- 5. Testing POST /api/light (Turn OFF) ---")
    body = b'{"state":"OFF"}'
    req = f"POST /api/light HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 200, "POST /api/light OFF -> 200")
    assert_eq(data["lightOn"], False, "lightOn: false")

    print("\n--- 6. Testing Error Handling (400 Bad Request & Invalid Value) ---")
    body = b'{"state":"INVALID"}'
    req = f"POST /api/light HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 400, "Invalid state returns 400")
    assert_eq(data["error"], "INVALID_VALUE", "error code is INVALID_VALUE")

    body = b'not json'
    req = f"POST /api/light HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 400, "Invalid JSON returns 400")
    assert_eq(data["error"], "BAD_REQUEST", "error code is BAD_REQUEST")

    print("\n--- 7. Testing Scenario: automation_on (409 Conflict) ---")
    STATE.set_scenario("automation_on")
    body = b'{"state":"ON"}'
    req = f"POST /api/light HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 409, "POST /api/light with automation ON returns 409")
    assert_eq(data["error"], "AUTOMATION_ENABLED", "error code is AUTOMATION_ENABLED")

    print("\n--- 8. Testing Scenario: gps_waiting ---")
    STATE.set_scenario("gps_waiting")
    req = b"GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n"
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 200, "GET /api/status -> 200")
    assert_eq(data["gpsValid"], False, "gpsValid is False")
    assert_eq(data["latitude"], None, "latitude is None")
    assert_eq(data["sunrise"], None, "sunrise is None")

    print("\n--- 9. Testing Scenario: command_fail (500) ---")
    STATE.set_scenario("command_fail")
    body = b'{"state":"ON"}'
    req = f"POST /api/light HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n\r\n".encode() + body
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 500, "command_fail returns 500")
    assert_eq(data["error"], "INTERNAL_ERROR", "error code is INTERNAL_ERROR")

    print("\n--- 10. Testing Scenario: esp_offline (500) ---")
    STATE.set_scenario("esp_offline")
    req = b"GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\n"
    code, hdrs, data, _ = TestableMockRequestHandler(req).get_response()
    assert_eq(code, 500, "esp_offline returns 500")
    assert_eq(data["error"], "INTERNAL_ERROR", "error code is INTERNAL_ERROR")

    print(f"\n========================================")
    print(f"Results: {passed}/{total} tests passed.")
    print(f"========================================")
    assert passed == total

if __name__ == "__main__":
    run_tests()
