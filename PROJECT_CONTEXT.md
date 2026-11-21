# Project Context — ESP32 Smart Streetlight (v2, simplified)

This is a restart. Ignore any earlier, larger dashboard design. Only what is written here is in scope.

## Goal
An ESP32 streetlight controller that gets location/date/time from GPS, calculates sunrise and sunset ON THE ESP32, and serves a very
simple web page from the ESP32 itself (no cloud, no external server). Users open the ESP32's IP in a browser on the same Wi-Fi network.

## Hardware (UNCHANGED — DO NOT CHANGE)
| Component | ESP32 Pin | Notes |
|---|---|---|
| GPS RX (ESP32 receives from GPS TX) | GPIO 16 | UART2, 9600 baud |
| GPS TX (ESP32 transmits to GPS RX) | GPIO 17 | UART2, 9600 baud |
| Relay IN1 | GPIO 23 | Active-HIGH: HIGH = ON, LOW = OFF |

ESP32 DevKit, NEO-6M GPS (TinyGPSPlus), 2-channel 5V relay module (channel 1 used), LED as streetlight prototype. No other hardware.
**There is NO LDR in this project.** Do not read GPIO34 and do not add any LDR, ambient-light or analog-sensor code, API fields or UI.

## Current code state
Existing firmware only does GPS diagnostics. Web server, API, sunrise/sunset calculation and automation are NOT implemented yet.
Inspect existing code before changing it; keep the working GPS code.

## CONFIRMED DECISIONS (from the project owner)
1. The webpage shows ONLY: latitude, longitude, sunrise time, sunset time, ESP32 connection status, an ON button, an OFF button, and the actual LED/relay state (ON/OFF).
2. Automatic control uses SUNRISE/SUNSET FROM GPS ONLY. The project does not use an LDR at all.
3. Automation exists in firmware and is switched on/off by a setting in the code: `AUTOMATION_ENABLED` in `config.h` (compile-time).
   - `AUTOMATION_ENABLED = true`: ESP32 switches the LED automatically (ON from sunset to sunrise). The page buttons are DISABLED; the page tells the user manual control is disabled because automation is on. `POST /api/light` returns 409.
   - `AUTOMATION_ENABLED = false`: ESP32 never switches the LED by itself. Only the page buttons control it.
   - There is NO AUTO/MANUAL selector on the page and no mode endpoint.
4. Sunrise/sunset is calculated on the ESP32 from GPS latitude/longitude/date (never in JavaScript, never invented). Times are IST (UTC+05:30, fixed), format "HH:MM".
5. The page must not show fake live data. Unknown values display "--".
6. Everything runs locally: page served from the ESP32 (LittleFS preferred, PROGMEM as documented fallback). No cloud, no CDN.
7. Two-person job: Person A = frontend (+ mock server + demo mode), Person B = firmware.

## DEFAULTS THE AGENT MUST NOT CHANGE WITHOUT ASKING (flagged for owner confirmation)
These were not specified by the owner; they are minimal safe defaults. If the owner wants something different he will edit this section.
- D1. At boot the LED/relay output is set LOW (OFF) before anything else.
- D2. With automation ON and GPS date/time/location not yet valid (or sunrise/sunset null, e.g. polar day/night), automation does NOT change the relay (it stays OFF after boot) until valid sunrise/sunset and time are available.
- D3. (removed: the LDR is not part of this project; see Confirmed Decision 2.)
- D4. When GPS is not valid the page shows "--" placeholders only (no extra "Waiting for GPS fix" message, since it was not requested).
- D5. "ESP32 connection" on the page = whether the browser can successfully reach `/api/status` (derived from polling), not a field from the ESP32.

## Page requirements (frontend)
Elements, nothing more: title "SMART STREETLIGHT" (subtitle "GPS-Based Intelligent Lighting System" is allowed as plain text); ESP32 connection indicator (Connected / Disconnected); Latitude; Longitude; Sunrise (IST); Sunset (IST); LED state (ON/OFF, from the ESP32); "Turn ON" and "Turn OFF" buttons; a short notice shown only when `automationEnabled` is true.
Behaviour:
- Poll `GET /api/status` every ~1.5 s with `fetch`, no page reloads.
- Buttons call `POST /api/light`. The UI shows the new LED state only after the ESP32 confirms it (response `ok:true` and the next status). Loading state while pending; error message on failure. Never optimistic.
- Buttons disabled when automation is on, or ESP32 is disconnected, or a command is in flight.
- Handle: ESP32 unreachable, failed requests, failed commands, null values.
- Demo mode (OFF by default, clearly labelled "DEMO MODE — SAMPLE DATA, NOT FROM HARDWARE"), fully separate from real mode.
Design: modern smart-city/infrastructure look — dark navy/charcoal, subtle bordered cards, cyan/teal accents, green = ON/connected, amber = warning/notice, red = errors only. System font stack, good spacing, subtle transitions, responsive (desktop/tablet/mobile), no heavy animation or bright gradients, no clutter. Simple, but it must look like a real product.
Size: frontend under ~30 KB total.

## Deliverables (whole project)
Frontend files; mock server; ESP32 firmware implementing the contract; documentation for: running the frontend locally, hosting on ESP32, integrating with the firmware, testing endpoints. Do not claim hardware integration is complete until actually tested.
