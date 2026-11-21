# API Contract — Frontend <-> ESP32 (v2.1)

SOURCE OF TRUTH. Status: UPDATED, UNTESTED ON HARDWARE. Change process: edit this file, bump version, add a Changelog line, notify teammate.

## General
- Same origin as the page (served by the ESP32). The frontend uses relative paths: `/api/status`, `/api/light`, `/api/light/on`, `/api/light/off`, `/api/mode`.
- JSON in and out, `Content-Type: application/json`, including errors.
- CORS (for dev when the page is NOT served by the ESP32): `Access-Control-Allow-Origin: *`, `Access-Control-Allow-Methods: GET, POST, OPTIONS`, `Access-Control-Allow-Headers: Content-Type`; `OPTIONS` -> 204.
- No authentication (local network prototype). Never expose to the internet.
- Unknown/invalid value -> `null`. Never a placeholder number.

## GET /api/status -> 200
```json
{
  "gpsValid": true,
  "latitude": 12.9716,
  "longitude": 77.5946,
  "sunrise": "06:15",
  "sunset": "17:55",
  "lightOn": false,
  "automationEnabled": true,
  "mode": "AUTO"
}
```
(Example values only, NOT real readings.)

| Field | Type | Meaning |
|---|---|---|
| gpsValid | bool | TinyGPSPlus location.isValid() |
| latitude, longitude | number\|null | decimal degrees; null unless gpsValid |
| sunrise, sunset | string\|null | IST "HH:MM", calculated on the ESP32; null unless GPS location+date valid (or polar day/night) |
| lightOn | bool | ACTUAL relay output state (GPIO23 HIGH = true), read back from the pin, not from a requested variable |
| automationEnabled | bool | true if system is in AUTO mode, false if in MANUAL mode |
| mode | string | "AUTO" or "MANUAL" (runtime controllable) |

Frontend: null -> show "--". "ESP32 connection" = browser can reach this endpoint (success = Connected; timeout/error = Disconnected).
Unknown extra fields must be ignored.

## Streetlight Control (Manual Mode Only)
Allowed only when `mode` is `"MANUAL"` (`automationEnabled` is false). If called in AUTO mode, returns HTTP 409 `AUTOMATION_ENABLED`.

### 1. POST /api/light
Request: `{ "state": "ON" }` or `{ "state": "OFF" }`  
Success 200: `{ "ok": true, "lightOn": true|false }`

### 2. GET /api/light/on
Success 200: `{ "ok": true, "lightOn": true }`

### 3. GET /api/light/off
Success 200: `{ "ok": true, "lightOn": false }`

## Mode Switching

### 1. POST /api/mode
Request: `{ "mode": "AUTO" }` or `{ "mode": "MANUAL" }`  
Success 200: `{ "ok": true, "mode": "AUTO"|"MANUAL", "automationEnabled": true|false }`

### 2. GET /api/mode/auto
Sets mode to AUTO.  
Success 200: `{ "ok": true, "mode": "AUTO", "automationEnabled": true }`

### 3. GET /api/mode/manual
Sets mode to MANUAL.  
Success 200: `{ "ok": true, "mode": "MANUAL", "automationEnabled": false }`

## Errors
Shape: `{ "ok": false, "error": "CODE", "message": "Human readable text" }`

| HTTP | error | When |
|---|---|---|
| 400 | BAD_REQUEST | Body missing / not valid JSON |
| 400 | INVALID_VALUE | state is not ON or OFF, or mode is not AUTO or MANUAL |
| 404 | NOT_FOUND | Unknown route |
| 405 | METHOD_NOT_ALLOWED | Wrong HTTP verb |
| 409 | AUTOMATION_ENABLED | Light command attempted while mode is AUTO |
| 500 | INTERNAL_ERROR | Unexpected fault |

## Frontend behaviour required
- Poll status every ~1500 ms; request timeout ~3 s; no overlapping requests. After 3 consecutive failures show Disconnected and clear live values to "--". Recover automatically.
- Mode Selector: toggling between AUTO and MANUAL sends mode switch request and refreshes status.
- When AUTO: manual Turn ON/Turn OFF buttons are disabled with notice displayed.
- When MANUAL: manual Turn ON/Turn OFF buttons are enabled; notice hidden.
- Clicking Turn ON calls `GET /api/light/on` (or `POST /api/light`). Clicking Turn OFF calls `GET /api/light/off`.
- Loading spinner active only while request is in flight. LED state updated directly from response and next status poll.

## Test with curl
```bash
# Check status
curl http://<esp-ip>/api/status

# Switch to MANUAL mode
curl -X POST http://<esp-ip>/api/mode -H "Content-Type: application/json" -d '{"mode":"MANUAL"}'

# Turn light ON via GET or POST
curl http://<esp-ip>/api/light/on
curl -X POST http://<esp-ip>/api/light -H "Content-Type: application/json" -d '{"state":"ON"}'

# Switch back to AUTO mode
curl http://<esp-ip>/api/mode/auto
```

## Changelog
- v2.1 — Added runtime AUTO / MANUAL mode switching (`POST /api/mode`, `GET /api/mode/auto`, `GET /api/mode/manual`) and direct light endpoints (`GET /api/light/on`, `GET /api/light/off`). Added `mode` field to `/api/status`.
- v2.0 — restart. Simplified: removed /api/mode and all LDR/ambient-light, GPS-detail and mode fields (the project has no LDR); added `automationEnabled`.
