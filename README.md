# SunSync-Streetlight: GPS-Based Intelligent Lighting System

A standalone, solar-aware ESP32 smart streetlight controller that determines exact geographic coordinates, date, and time via an on-board GPS module, calculates solar sunrise and sunset directly on the microcontroller (no cloud or third-party APIs required), and serves an embedded real-time control dashboard over local Wi-Fi.

---

## 🌟 Key Highlights

- **Edge Solar Calculations**: Sunrise and sunset times (in Indian Standard Time, UTC+05:30) are computed directly on the ESP32 using solar position algorithms—eliminating reliance on external cloud services or internet gateways.
- **Pure GPS Automation**: Uses GPS date, time, and coordinates exclusively. **No LDR (Light Dependent Resistor)** or fragile analog light sensors.
- **Embedded Web Server**: Serves a responsive, lightweight, accessible dashboard directly from ESP32 LittleFS.
- **Zero Cloud / Zero External Assets**: 100% local network operation; uses vanilla HTML5/CSS3/JavaScript with system fonts and inline SVGs (< 30 KB total uncompressed).
- **Safe Automation Lockout**: Automatic mode locks manual overrides (HTTP 409 Conflict) and switches the light automatically between sunset and sunrise.
- **Fail-Safe Operation**: Defaults relay to `OFF` (LOW) at boot and upon GPS fix loss. Displays connection alerts when disconnected.

---

## 🛠 Hardware Configuration

| Component | ESP32 Pin | Protocol / Logic | Notes |
|---|---|---|---|
| **GPS Module (NEO-6M)** | **GPIO 16 (RX2)** | UART2 @ 9600 baud | ESP32 receives NMEA data from GPS TX |
| **GPS Module (NEO-6M)** | **GPIO 17 (TX2)** | UART2 @ 9600 baud | ESP32 transmit |
| **Relay Module (Ch 1)** | **GPIO 23** | Active-HIGH (3.3V / 5V) | `HIGH = ON`, `LOW = OFF` |
| **Status Prototype** | LED | Connected to Relay Ch 1 | Streetlight fixture prototype |

> **Note**: Pin definitions and relay logic (`HIGH = ON`, `LOW = OFF`) are hardware invariants.

---

## 📁 Repository Structure

```text
SunSync-Streetlight/
├── frontend/                     # Web dashboard assets (served via LittleFS)
│   ├── index.html                # Semantic, accessible HTML5 dashboard (< 6 KB)
│   ├── styles.css                # Dark-themed infrastructure styles (< 9 KB)
│   ├── app.js                    # Non-overlapping poller & command dispatcher (< 8 KB)
│   ├── config.js                 # API base and timing configurations
│   └── demo.js                   # Isolated client-side demo mode engine
├── tools/                        # Development and simulation tools
│   ├── mock_server.py            # Zero-dependency Python 3 mock server with full contract
│   ├── test_mock_in_memory.py    # Automated HTTP wire-format test suite (28/28 tests passing)
│   └── test_mock_server.py       # Socket-level integration test runner
├── docs/                         # Specifications & guides
│   ├── FRONTEND_GUIDE.md         # Frontend architecture, testing & LittleFS deployment
│   ├── WORK_SPLIT.md             # Responsibilities for Person A & Person B
│   └── CHANGE_REQUESTS.md        # Tracked requests between frontend and firmware
├── AGENTS.md                     # Collaboration guidelines & hard architectural constraints
├── API_CONTRACT.md               # Source of truth: REST API schema (v2.0)
├── PROJECT_CONTEXT.md            # Hardware decisions, design specs, and requirements
└── README.md                     # Project overview and documentation
```

---

## 📡 REST API Contract (v2.0)

All communication between the frontend interface and the ESP32 occurs over two endpoints:

### 1. `GET /api/status`
Returns real-time status and telemetry:
```json
{
  "gpsValid": true,
  "latitude": 12.9716,
  "longitude": 77.5946,
  "sunrise": "06:15",
  "sunset": "17:55",
  "lightOn": false,
  "automationEnabled": false
}
```
*Unknown values are returned as `null` and displayed as `"--"` in the dashboard.*

### 2. `POST /api/light`
Actuates the relay when automation is disabled:
- **Request**: `{"state": "ON"}` or `{"state": "OFF"}`
- **Success (200)**: `{"ok": true, "lightOn": true}` (read back from hardware pin GPIO23)
- **Lockout (409)**: Returned if `automationEnabled: true`

---

## 💻 Quick Start & Development

### Running the Frontend Locally (Mock Server)
The repository includes a Python 3 mock server simulating the ESP32 hardware and API contract without dependencies:

```bash
# Start the mock server with default scenario (gps_ok)
python tools/mock_server.py

# Or run with specific scenarios
python tools/mock_server.py --scenario gps_waiting
python tools/mock_server.py --scenario automation_on
```
Open [http://localhost:8080](http://localhost:8080) to interact with the dashboard.

### Demo Mode
Test the dashboard without running any server:
- Open `frontend/index.html?demo=1` in any modern web browser.
- Displays an amber banner clearly indicating sample data is in use, allowing scenario simulation and UI verification.

### Running Wire-Format Test Suite
```bash
python tools/test_mock_in_memory.py
```

---

## 🚀 Firmware Deployment (ESP32 LittleFS)

1. Copy the contents of `frontend/` into `firmware/smart_streetlight/data/`.
2. Ensure `API_BASE` in `config.js` is set to `""` (relative paths).
3. Build and upload filesystem image via PlatformIO (`pio run --target uploadfs`) or Arduino IDE LittleFS plugin.
4. Compile and flash the firmware to your ESP32 DevKit.

---

## 📄 License
This project is open-source under the MIT License.
