# Prompt for Person B — ESP32 Firmware (paste into Antigravity)

You are an expert embedded systems engineer (ESP32, Arduino framework, C++). You are **Person B** on a two-person team building a SIMPLE ESP32 Smart Streetlight system. Person A builds the web page. You build the firmware.

## Step 0 — Read first (mandatory)
Read fully: `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, `docs/API_CONTRACT.md`, `docs/WORK_SPLIT.md`.
**Inspect the existing firmware** (currently GPS diagnostics only, TinyGPSPlus on UART2). Do not overwrite working code.
Then reply with: (1) what you understood, (2) proposed firmware file structure and how you reuse the existing GPS code, (3) proposed libraries (prefer built-ins: WiFi.h, WebServer.h, LittleFS.h, TinyGPSPlus; justify anything else, e.g. ArduinoJson), (4) any doubts or conflicts.
**Do not assume anything. If something is unclear, ask me and wait. Do not write code until I approve.**

## Ownership
Edit only: `firmware/**`, `docs/FIRMWARE_GUIDE.md`, `.gitignore`. Never edit `frontend/**`. `firmware/smart_streetlight/data/` is a COPY of `frontend/`; copy, never hand-edit. Contract changes/requests follow AGENTS.md.

## Hardware (DO NOT CHANGE)
GPS RX=GPIO16 (ESP32 receives from GPS TX), GPS TX=GPIO17, UART2 @ 9600 (NEO-6M, TinyGPSPlus). Relay IN1=GPIO23, active-HIGH (HIGH=ON). No other hardware. There is NO LDR in this project: do not read GPIO34 or add any LDR/ambient-light code.

## What to build (implement `docs/API_CONTRACT.md` exactly)
1. **Wi-Fi**: station mode; credentials only in `secrets.h` (gitignored) + committed `secrets.example.h`; print IP on Serial; auto-reconnect; no blocking delays — GPS bytes must keep being read.
2. **GPS**: reuse/extend existing TinyGPSPlus code. Do not assume a fix at boot. In JSON, `latitude`/`longitude` are `null` unless location is valid.
3. **IST**: derive IST (UTC+05:30 fixed) from GPS date + UTC time, correctly handling date rollover.
4. **Sunrise/sunset ON THE ESP32** (own files e.g. `sun_calc.h/.cpp`): NOAA/standard solar algorithm from latitude, longitude, date (zenith 90.833°), output IST "HH:MM". Handle polar day/night (null, no crash). Recompute only when the date or position meaningfully changes. Include a Serial self-test with a known reference location/date and document how to compare with an independent reference. `sunrise`/`sunset` are `null` unless valid.
5. **Relay**: GPIO23 active-HIGH. Set LOW (OFF) FIRST at boot (default D1). `lightOn` in responses is READ BACK from the pin state.
6. **Automation flag**: `config.h` has `#define AUTOMATION_ENABLED true|false` (compile-time).
   - true: ESP32 switches the relay automatically: ON from sunset until sunrise (IST), OFF otherwise, using sunrise/sunset only. While GPS/time/sunrise/sunset are not valid, do not change the relay (default D2). Only write the pin when the desired state changes. `POST /api/light` -> 409 `AUTOMATION_ENABLED`.
   - false: the firmware NEVER switches the relay by itself; only `POST /api/light` does.
   - Report the flag as `automationEnabled` in `/api/status`.
7. **Web server**: serve `frontend/` files from LittleFS at `/` with correct MIME types (document the PROGMEM fallback in the guide; do not implement unless asked). Implement only `GET /api/status` and `POST /api/light`, with the exact JSON, HTTP codes (200/400/404/405/409/500) and error shape from the contract. CORS + OPTIONS as specified. Validate bodies (size limit, parse errors -> 400); never crash on bad input; never block GPS reading.
8. **Debug**: Serial 115200 with a `DEBUG` flag: Wi-Fi/IP, GPS validity changes, sunrise/sunset results, relay changes (and why: automation vs button), API requests.
9. **Docs** `docs/FIRMWARE_GUIDE.md`: libraries + versions, board settings (ESP32 Dev Module, partition scheme with LittleFS), setting up `secrets.h`, how to switch `AUTOMATION_ENABLED`, LittleFS upload of `data/` (Arduino IDE plugin or PlatformIO `uploadfs`), copying frontend into `data/`, curl tests with expected output, how to verify sunrise/sunset, flash/RAM usage.

## Hard rules
- No pin changes, no relay-logic change, no new hardware, no extra endpoints/fields. Don't add features; ask if something seems missing.
- No fabricated values: unknown -> `null`. Contract JSON examples are examples only; never hardcode coordinates/times as real readings.
- No credentials in tracked files.
- Report honestly, separating "compiles", "tested on hardware", "not tested". If you cannot compile/flash here, say so. Never claim hardware integration works before it is tested on the real ESP32.
- Preserve working GPS diagnostic code. Plan before major changes; don't overwrite unrelated code.

## Method
Step 0 and wait. Milestones: (a) Wi-Fi + server skeleton + `/api/status` with real GPS lat/lon -> (b) relay + `POST /api/light` + `AUTOMATION_ENABLED` handling -> (c) sun_calc + IST + automation logic -> (d) LittleFS hosting + CORS + validation + docs. After each, give a short summary, exact test steps (curl + expected output) and what is still unverified. If requirements change later, update the relevant doc in the same task.
