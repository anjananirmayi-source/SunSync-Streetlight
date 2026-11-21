# Work Split — Roles and Ownership

## Person A — Frontend & Simulation
- **Ownership**:
  - `frontend/**` (`index.html`, `styles.css`, `app.js`, `config.js`, `demo.js`)
  - `tools/**` (`mock_server.py`, test suites)
  - `docs/FRONTEND_GUIDE.md`
- **Responsibilities**:
  - Web UI design under 30 KB limit with system fonts and dark smart-city theme.
  - Client-side polling and non-optimistic command dispatching.
  - Isolated demo mode with persistent watermark banner.
  - Python mock server adhering strictly to `API_CONTRACT.md`.

## Person B — Firmware & Hardware Integration
- **Ownership**:
  - `firmware/**`
  - `docs/FIRMWARE_GUIDE.md`
- **Responsibilities**:
  - GPS parsing over UART2 (GPIO 16/17) using TinyGPSPlus.
  - On-chip solar sunrise/sunset calculation (IST UTC+05:30).
  - Relay control on GPIO 23 (active-HIGH, default LOW at boot).
  - LittleFS static file hosting and REST API implementation.
  - Automation logic via `AUTOMATION_ENABLED` compile-time flag.

## Shared Contract
- `API_CONTRACT.md` is the single source of truth. Changes require version bumping and mutual alignment.
