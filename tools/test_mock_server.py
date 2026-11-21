"""
Self-test suite for ESP32 Smart Streetlight Mock Server & API Contract v2.0
"""
import sys
import os
import time
import subprocess
import json
import urllib.request
import urllib.error

def run_tests():
    host = "127.0.0.1"
    port = 8899
    base_url = f"http://{host}:{port}"
    script_dir = os.path.dirname(os.path.abspath(__file__))
    mock_server_script = os.path.join(script_dir, "mock_server.py")

    print(f"Starting mock server on {base_url}...")
    proc = subprocess.Popen([
        sys.executable, mock_server_script,
        "--host", host,
        "--port", str(port),
        "--scenario", "gps_ok"
    ])

    time.sleep(1.0)
    passed = 0
    total = 0

    def assert_test(cond, name):
        nonlocal passed, total
        total += 1
        if cond:
            print(f"  [PASS] {name}")
            passed += 1
        else:
            print(f"  [FAIL] {name}")

    try:
        # 1. Test Static Files
        print("\n--- 1. Testing Static File Serving ---")
        for asset in ["/", "/styles.css", "/app.js", "/config.js", "/demo.js"]:
            req = urllib.request.Request(f"{base_url}{asset}")
            with urllib.request.urlopen(req) as resp:
                assert_test(resp.status == 200, f"Serve {asset} (HTTP 200)")

        # 2. Test CORS OPTIONS
        print("\n--- 2. Testing CORS OPTIONS Preflight ---")
        req = urllib.request.Request(f"{base_url}/api/status", method="OPTIONS")
        with urllib.request.urlopen(req) as resp:
            assert_test(resp.status == 204, "OPTIONS /api/status -> 204")
            assert_test(resp.headers.get("Access-Control-Allow-Origin") == "*", "CORS Allow-Origin *")

        # 3. Test GET /api/status in scenario gps_ok
        print("\n--- 3. Testing GET /api/status (Scenario: gps_ok) ---")
        req = urllib.request.Request(f"{base_url}/api/status")
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode())
            assert_test(data.get("gpsValid") is True, "gpsValid is true")
            assert_test(data.get("latitude") == 12.9716, "latitude matches")
            assert_test(data.get("longitude") == 77.5946, "longitude matches")
            assert_test(data.get("sunrise") == "06:15", "sunrise matches")
            assert_test(data.get("sunset") == "17:55", "sunset matches")
            assert_test(data.get("lightOn") is False, "lightOn is false initially")
            assert_test(data.get("automationEnabled") is False, "automationEnabled is false")

        # 4. Test POST /api/light commands
        print("\n--- 4. Testing POST /api/light (Turn ON & OFF) ---")
        # Turn ON
        req = urllib.request.Request(
            f"{base_url}/api/light",
            data=json.dumps({"state": "ON"}).encode(),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode())
            assert_test(resp.status == 200 and data.get("ok") is True and data.get("lightOn") is True, "Turn ON -> 200 lightOn:true")

        # Verify status confirms ON
        req = urllib.request.Request(f"{base_url}/api/status")
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode())
            assert_test(data.get("lightOn") is True, "Readback confirms lightOn is true")

        # Turn OFF
        req = urllib.request.Request(
            f"{base_url}/api/light",
            data=json.dumps({"state": "OFF"}).encode(),
            headers={"Content-Type": "application/json"}
        )
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode())
            assert_test(resp.status == 200 and data.get("ok") is True and data.get("lightOn") is False, "Turn OFF -> 200 lightOn:false")

        # 5. Test Command Error Handling
        print("\n--- 5. Testing POST /api/light Validation Errors ---")
        # Invalid state
        req = urllib.request.Request(
            f"{base_url}/api/light",
            data=json.dumps({"state": "INVALID"}).encode(),
            headers={"Content-Type": "application/json"}
        )
        try:
            urllib.request.urlopen(req)
            assert_test(False, "Invalid state should fail with 400")
        except urllib.error.HTTPError as e:
            err = json.loads(e.read().decode())
            assert_test(e.code == 400 and err.get("error") == "INVALID_VALUE", "Invalid state -> 400 INVALID_VALUE")

        # Bad JSON
        req = urllib.request.Request(
            f"{base_url}/api/light",
            data=b"not json",
            headers={"Content-Type": "application/json"}
        )
        try:
            urllib.request.urlopen(req)
            assert_test(False, "Bad JSON should fail with 400")
        except urllib.error.HTTPError as e:
            err = json.loads(e.read().decode())
            assert_test(e.code == 400 and err.get("error") == "BAD_REQUEST", "Bad JSON -> 400 BAD_REQUEST")

        # 6. Test Scenario: automation_on -> 409 Conflict
        print("\n--- 6. Testing Scenario: automation_on (409 Conflict) ---")
        # Switch scenario
        req = urllib.request.Request(
            f"{base_url}/api/mock/scenario",
            data=json.dumps({"scenario": "automation_on"}).encode(),
            headers={"Content-Type": "application/json"}
        )
        urllib.request.urlopen(req)

        # Status check
        req = urllib.request.Request(f"{base_url}/api/status")
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode())
            assert_test(data.get("automationEnabled") is True, "automationEnabled is true in status")

        # Attempt POST /api/light -> must return 409
        req = urllib.request.Request(
            f"{base_url}/api/light",
            data=json.dumps({"state": "ON"}).encode(),
            headers={"Content-Type": "application/json"}
        )
        try:
            urllib.request.urlopen(req)
            assert_test(False, "POST /api/light with automation ON must return 409")
        except urllib.error.HTTPError as e:
            err = json.loads(e.read().decode())
            assert_test(e.code == 409 and err.get("error") == "AUTOMATION_ENABLED", "Manual command while automation ON -> 409 AUTOMATION_ENABLED")

        # 7. Test Scenario: gps_waiting
        print("\n--- 7. Testing Scenario: gps_waiting ---")
        req = urllib.request.Request(
            f"{base_url}/api/mock/scenario",
            data=json.dumps({"scenario": "gps_waiting"}).encode(),
            headers={"Content-Type": "application/json"}
        )
        urllib.request.urlopen(req)

        req = urllib.request.Request(f"{base_url}/api/status")
        with urllib.request.urlopen(req) as resp:
            data = json.loads(resp.read().decode())
            assert_test(data.get("gpsValid") is False, "gpsValid is false")
            assert_test(data.get("latitude") is None, "latitude is null")
            assert_test(data.get("longitude") is None, "longitude is null")
            assert_test(data.get("sunrise") is None, "sunrise is null")
            assert_test(data.get("sunset") is None, "sunset is null")

        # 8. Test Scenario: command_fail
        print("\n--- 8. Testing Scenario: command_fail ---")
        req = urllib.request.Request(
            f"{base_url}/api/mock/scenario",
            data=json.dumps({"scenario": "command_fail"}).encode(),
            headers={"Content-Type": "application/json"}
        )
        urllib.request.urlopen(req)

        req = urllib.request.Request(
            f"{base_url}/api/light",
            data=json.dumps({"state": "ON"}).encode(),
            headers={"Content-Type": "application/json"}
        )
        try:
            urllib.request.urlopen(req)
            assert_test(False, "command_fail scenario must return 500")
        except urllib.error.HTTPError as e:
            err = json.loads(e.read().decode())
            assert_test(e.code == 500 and err.get("error") == "INTERNAL_ERROR", "command_fail -> 500 INTERNAL_ERROR")

        # 9. Test Scenario: esp_offline
        print("\n--- 9. Testing Scenario: esp_offline ---")
        req = urllib.request.Request(
            f"{base_url}/api/mock/scenario",
            data=json.dumps({"scenario": "esp_offline"}).encode(),
            headers={"Content-Type": "application/json"}
        )
        urllib.request.urlopen(req)

        req = urllib.request.Request(f"{base_url}/api/status")
        try:
            urllib.request.urlopen(req)
            assert_test(False, "esp_offline status must return 500")
        except urllib.error.HTTPError as e:
            assert_test(e.code == 500, "esp_offline -> 500")

    finally:
        print("\nStopping Mock Server...")
        proc.terminate()
        proc.wait()

    print(f"\n========================================")
    print(f"Results: {passed}/{total} tests passed.")
    print(f"========================================")
    if passed == total:
        print("ALL TESTS PASSED!")
    else:
        sys.exit(1)

if __name__ == "__main__":
    run_tests()
