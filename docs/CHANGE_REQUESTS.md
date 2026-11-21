# Change Requests

Cross-boundary requests between Person A (Frontend) and Person B (Firmware) should be recorded here.

---

## CR-20261121-01: Runtime AUTO/MANUAL Mode Selector & Direct Light Endpoints
- **From**: Person B (Approved by Human Owner)
- **Status**: Completed
- **Description**:
  1. Added runtime AUTO / MANUAL mode selector to the dashboard.
  2. When AUTO mode is active: manual ON/OFF buttons are disabled and clear message is displayed.
  3. When MANUAL mode is selected: manual ON/OFF buttons are enabled and notice is hidden.
  4. Added `GET /api/light/on` and `GET /api/light/off` endpoints for direct light state toggling.
  5. Added `POST /api/mode` (`{"mode":"AUTO"|"MANUAL"}`), `GET /api/mode/auto`, and `GET /api/mode/manual` endpoints.
  6. Updated `GET /api/status` to include `"mode": "AUTO"|"MANUAL"`.
  7. Ensured loading indicator is active only while request is in flight and never spins indefinitely.
  8. Preserved all existing SolarSync UI themes, layout, responsive design, and sunset-to-sunrise astronomical logic.
- **Affected Docs/Contract**:
  - `API_CONTRACT.md` bumped to v2.1.
  - `firmware/smart_streetlight/relay_controller.h/.cpp` updated with runtime mode control.
  - `firmware/smart_streetlight/web_server.cpp` updated with v2.1 endpoints.
  - `tools/mock_server.py` updated with v2.1 endpoints and default port 8000.
  - `frontend/index.html`, `frontend/styles.css`, `frontend/app.js` updated.
  - `firmware/smart_streetlight/data/` synchronized with updated frontend assets.
