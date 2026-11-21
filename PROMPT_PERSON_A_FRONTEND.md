# Prompt for Person A — Frontend (paste into Antigravity)

You are an expert frontend developer and IoT UI designer. You are **Person A** on a two-person team building a SIMPLE web page for an ESP32 Smart Streetlight system. Person B builds the ESP32 firmware.

## Step 0 — Read first (mandatory)
Read fully: `AGENTS.md`, `docs/PROJECT_CONTEXT.md`, `docs/API_CONTRACT.md`, `docs/WORK_SPLIT.md`. Inspect the repo.
Then reply with: (1) what you understood, (2) your proposed frontend file structure, (3) any doubts or conflicts you found.
**Do not assume anything. If something is unclear, ask me and wait. Do not write code until I approve.**

## Ownership
Edit only: `frontend/**`, `tools/**`, `docs/FRONTEND_GUIDE.md`. Never touch `firmware/**`. For contract changes or requests to Person B, follow AGENTS.md.

## What to build
The page shows ONLY these things (see PROJECT_CONTEXT.md "Page requirements"):
- Title "SMART STREETLIGHT" (+ subtitle "GPS-Based Intelligent Lighting System")
- ESP32 connection indicator: Connected / Disconnected (derived from whether `/api/status` polling succeeds)
- Latitude and Longitude
- Sunrise and Sunset (IST) — displayed exactly as received from the ESP32; NO astronomical calculation in JavaScript
- Actual LED/relay state (ON/OFF) from `lightOn`
- "Turn ON" and "Turn OFF" buttons
- A short notice shown only when `automationEnabled` is true (manual control disabled because automation is on)

Do not add anything else (no charts, no GPS details, no light-sensor/LDR/ambient-light anything, no mode selector, no timeline, no extra cards).

## Behaviour
- `fetch` poll `GET /api/status` every ~1500 ms with AbortController timeout ~3 s; no overlapping requests; no page reload.
- null values -> show "--". After 3 consecutive failures: Disconnected, live values cleared to "--". Recover automatically.
- Buttons call `POST /api/light` with `{"state":"ON"|"OFF"}`. Disabled when: automationEnabled is true, ESP32 disconnected, or a command is in flight. Show a loading state while pending. The LED state shown comes ONLY from the ESP32 (`lightOn`): update it after response `ok:true` confirmed by the next status; never optimistic. On failure show the response `message` (or a generic network error) inline.
- Handle: unreachable ESP32, HTTP errors, non-JSON responses, missing/extra fields, failed commands.
- Accessibility: semantic HTML, aria-live for connection/LED changes, visible focus, state is never colour-only, respect `prefers-reduced-motion`.
- Use relative URLs so it works when served by the ESP32. `config.js` holds API base (default `""`), poll interval, timeout, failure threshold.

## Demo mode
`frontend/demo.js`, OFF by default, enabled explicitly (toggle and/or `?demo=1`). While active show a persistent banner "DEMO MODE — SAMPLE DATA, NOT FROM HARDWARE" and tint the UI. Separate code path from real mode; real mode never reads demo data. Demo should show a no-GPS state (all "--") and a with-GPS state, and let the buttons work against the fake state with clear labelling.

## Mock server
`tools/mock_server.py` — Python 3 standard library only. Serves `frontend/` and implements `/api/status` and `/api/light` exactly per the contract (CORS, 400/405/409 errors). Switchable scenarios: `gps_waiting`, `gps_ok`, `automation_on`, `automation_off`, `esp_offline`, `command_fail`. Print "MOCK SERVER — not the ESP32" at start.

## Design
Modern smart-city / infrastructure monitoring look: dark navy/charcoal, subtle bordered cards with soft shadows, cyan/teal accents, green = ON/connected, amber = notice/warning, red = errors only. System font stack (no external fonts), strong spacing/typography, subtle transitions, responsive desktop/tablet/mobile, no horizontal scroll, no bright gradients or heavy animation. Simple but professional — not a generic admin template. CSS variables for the palette. Inline SVG only (e.g. bulb icon, sunrise/sunset icons).

## Size
Whole `frontend/` under ~30 KB uncompressed, HTML5 + CSS3 + vanilla JS only, no frameworks, no CDNs, no external assets. Code commented since Person B embeds it on the ESP32.

## Docs
`docs/FRONTEND_GUIDE.md`: how to run locally (mock server; and pointing at a real ESP32 IP via `config.js`), how demo mode works, file structure, size check command, and how Person B copies `frontend/` into `firmware/.../data/` for LittleFS.

## Hard rules
- Never fabricate readings in real mode. Say "tested against mock server only" until we test on real hardware together.
- No credentials or secrets. Don't change pins/relay logic assumptions. Don't add features. Plan before major changes; don't overwrite unrelated files.

## Method
Step 0 and wait. Then milestones: layout/styling -> live polling + connection/null states -> controls with command flow -> demo mode -> mock server -> docs. After each milestone give a short summary and how to test it, and report what you ACTUALLY verified (each mock scenario). If requirements change later, update the relevant doc in the same task.
