# API Contract — Frontend <-> ESP32 (v2.1)

SOURCE OF TRUTH. Status: PROPOSED, UNTESTED ON HARDWARE. Change process: edit this file, bump version, add a Changelog line, notify teammate.
Only two endpoints exist. Do not add more without asking the owner.

## General
- Same origin as the page (served by the ESP32). The frontend uses relative paths: `/api/status`, `/api/light`.
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
  "time": "12:34",
  "satellites": 7,
  "lightOn": false,
  "automationEnabled": true
}
```
(Example values only, NOT real readings.)

| Field | Type | Meaning |
|---|---|---|
| gpsValid | bool | TinyGPSPlus location.isValid() |
| latitude, longitude | number\|null | decimal degrees; null unless gpsValid |
| sunrise, sunset | string\|null | IST "HH:MM", calculated on the ESP32; null unless GPS location+date valid (or polar day/night) |
| time | string\|null | Current IST time "HH:MM" derived from GPS UTC + offset; null unless GPS time is valid |
| satellites | number\|null | Number of satellites currently tracked by TinyGPSPlus; null unless GPS time valid |
| lightOn | bool | ACTUAL relay output state (GPIO23 HIGH = true), read back from the pin, not from a requested variable |
| automationEnabled | bool | value of `AUTOMATION_ENABLED` in firmware config.h |

Frontend: null -> show "--". "ESP32 connection" = browser can reach this endpoint (success = Connected; timeout/error = Disconnected).
Unknown extra fields must be ignored.

## POST /api/light
Request: `{ "state": "ON" }` or `{ "state": "OFF" }`

Success 200: `{ "ok": true, "lightOn": true }` — `lightOn` is read back from the relay pin AFTER applying the command.

Allowed only when `automationEnabled` is false.

## Errors
Shape: `{ "ok": false, "error": "CODE", "message": "Human readable text" }`

| HTTP | error | When |
|---|---|---|
| 400 | BAD_REQUEST | Body missing / not valid JSON |
| 400 | INVALID_VALUE | state is not ON or OFF |
| 404 | NOT_FOUND | Unknown route |
| 405 | METHOD_NOT_ALLOWED | Wrong HTTP verb |
| 409 | AUTOMATION_ENABLED | POST /api/light while automation is on |
| 500 | INTERNAL_ERROR | Unexpected fault |

## Frontend behaviour required
- Poll status every ~1500 ms; request timeout ~3 s; no overlapping requests. After 3 consecutive failures show Disconnected and clear live values to "--" (do not leave old values looking live). Recover automatically.
- Command: loading state, then show the LED state only after `ok:true` and confirmation by the next status. On failure show `message`.

## Test with curl (Person B)
```
curl http://<esp-ip>/api/status
curl -X POST http://<esp-ip>/api/light -H "Content-Type: application/json" -d '{"state":"ON"}'
```

## Changelog
- v2.1 — added `time` (current IST HH:MM from GPS) and `satellites` (tracked satellite count) to `GET /api/status`.
- v2.0 — restart. Simplified: removed /api/mode and all LDR/ambient-light, GPS-detail and mode fields (the project has no LDR); added `automationEnabled`.
