# Frontend Guide — ESP32 Smart Streetlight (Person A)

This document explains how to develop, test, configure, and integrate the frontend web page for the ESP32 Smart Streetlight system.

---

## 1. Directory Structure

```text
sunlight_2_0/
├── frontend/                 # Static web files served by ESP32 LittleFS
│   ├── index.html            # Semantic accessible dashboard markup (< 7 KB)
│   ├── styles.css            # Responsive dark-theme infrastructure styles (< 6 KB)
│   ├── app.js                # Polling engine, command dispatcher, UI updater (< 9 KB)
│   ├── config.js             # API base URL and polling thresholds (< 1 KB)
│   └── demo.js               # Isolated Demo Mode engine with switchable states (< 4.5 KB)
├── tools/
│   └── mock_server.py        # Python 3 standard library mock server (no dependencies)
└── docs/
    └── FRONTEND_GUIDE.md     # This documentation
```

---

## 2. Running Locally with Mock Server

The mock server runs purely on the Python 3 standard library and requires no third-party packages.

### Start the Mock Server:
```bash
# From repository root:
python tools/mock_server.py
```
Open [http://localhost:8080](http://localhost:8080) in your browser.

### Simulated Scenarios:
You can start the server in any of the simulated scenarios:
```bash
# Normal operation with valid GPS fix (light OFF, automation OFF):
python tools/mock_server.py --scenario gps_ok

# Waiting for GPS fix (lat/lon and sunrise/sunset are null/--):
python tools/mock_server.py --scenario gps_waiting

# Automation active (buttons disabled, manual control returns 409):
python tools/mock_server.py --scenario automation_on

# ESP32 offline simulation (500 Internal Error / disconnect):
python tools/mock_server.py --scenario esp_offline

# Command failure simulation (relays error message on button press):
python tools/mock_server.py --scenario command_fail
```

You can also dynamically switch scenarios while the server is running by sending a POST request:
```bash
curl -X POST http://localhost:8080/api/mock/scenario \
  -H "Content-Type: application/json" \
  -d '{"scenario":"automation_on"}'
```

---

## 3. Connecting to a Real ESP32 via Wi-Fi

When testing the frontend locally while communicating with a real ESP32 on your local Wi-Fi:

1. Open [frontend/config.js](file:///c:/Users/Laptop/Desktop/robomanthan/sunlight_2_0/frontend/config.js).
2. Set `API_BASE` to the ESP32 IP address:
   ```javascript
   const CONFIG = {
     API_BASE: 'http://192.168.1.100', // Replace with ESP32 IP
     POLL_INTERVAL_MS: 1500,
     REQUEST_TIMEOUT_MS: 3000,
     MAX_CONSECUTIVE_FAILURES: 3
   };
   ```
3. Open `frontend/index.html` via any local HTTP server (or the mock server).
4. **Important**: When uploading the frontend to the ESP32 LittleFS, leave `API_BASE: ''` (empty string) so requests remain relative.

---

## 4. Demo Mode

Demo Mode allows showcasing the interface without any hardware or mock server running.
* **Persistent Warning Banner**: Displays `"DEMO MODE — SAMPLE DATA, NOT FROM HARDWARE"`.
* **Separate Execution**: Real polling is paused; real mode never reads demo data.
* **How to Activate**:
  * Click the **"Demo Mode"** button in the dashboard footer, OR
  * Open the URL with query parameter `?demo=1` (e.g. `http://localhost:8080/?demo=1`).
* **Scenarios in Demo Mode**:
  * **GPS Active & Valid**: Displays simulated coordinates (12.9716, 77.5946) and IST solar times (06:15, 17:55). Buttons toggle state.
  * **GPS Waiting**: Coordinates and solar times display `"--"`.
  * **Automation Active**: Relay shows ON, buttons are disabled, amber notice is visible.

---

## 5. Verifying Frontend Size (< 30 KB Limit)

The entire `frontend/` directory is constrained to under 30 KB uncompressed.

### Size Check Command (PowerShell):
```powershell
Get-ChildItem frontend | Select-Object Name, Length; (Get-ChildItem frontend | Measure-Object -Property Length -Sum).Sum
```

---

## 6. Handoff for Person B (Firmware Integration)

To embed the frontend into ESP32 LittleFS:

1. Copy the entire contents of `frontend/` into the firmware's LittleFS data directory:
   ```bash
   cp frontend/* firmware/smart_streetlight/data/
   ```
2. Verify that `config.js` in `data/` has `API_BASE: ''`.
3. Use the Arduino LittleFS upload tool (e.g. `ESP32 LittleFS Filesystem Uploader`) or PlatformIO:
   ```bash
   pio run --target uploadfs
   ```
4. Web server routes required on ESP32:
   * `/` -> `index.html` (MIME: `text/html`)
   * `/styles.css` -> `styles.css` (MIME: `text/css`)
   * `/app.js` -> `app.js` (MIME: `application/javascript`)
   * `/config.js` -> `config.js` (MIME: `application/javascript`)
   * `/demo.js` -> `demo.js` (MIME: `application/javascript`)
