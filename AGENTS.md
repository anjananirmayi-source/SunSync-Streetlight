# AGENTS.md — Rules for every AI agent working in this repo (v2, simplified)

Two humans, each with their own Antigravity agent, work in parallel. Read first, in this order:
1. docs/PROJECT_CONTEXT.md — what we build, hardware, confirmed decisions, constraints
2. docs/API_CONTRACT.md    — the ONLY interface between frontend and firmware (source of truth)
3. docs/WORK_SPLIT.md      — who owns which files

## Non-negotiable rules
- Inspect existing files first. Explain your proposed file structure/changes BEFORE major edits. Never overwrite unrelated code.
- Edit only files your person owns (WORK_SPLIT.md). Need something from the other person? Append to docs/CHANGE_REQUESTS.md.
- docs/API_CONTRACT.md is the single source of truth. To change it: edit the contract first, bump its version, add a Changelog line, and tell the human so they can inform their teammate.
- Do NOT change pins: GPS RX=GPIO16, GPS TX=GPIO17, Relay IN1=GPIO23. Do NOT change relay logic (HIGH = ON). Do NOT invent hardware.
- There is NO LDR in this project. Do not read, configure, mention or add any LDR / ambient-light / analog-sensor code, API fields or UI.
- THIS VERSION IS DELIBERATELY SIMPLE. Do not add features, sections, endpoints or fields that are not in the docs. If you think something is missing, ASK the human; do not assume.
- If anything is unclear or two documents disagree, STOP and ask the human before continuing.
- Never fabricate GPS or sunrise/sunset values and present them as real. Unknown = null in the API and "--" on the page. Demo data must be clearly labelled DEMO.
- No Wi-Fi credentials or secrets in tracked files (secrets.h is gitignored; secrets.example.h is committed).
- Do not claim hardware integration works until tested on a real ESP32. Say "untested on hardware" until then.
- Lightweight only: HTML/CSS/vanilla JS, no frameworks, no CDNs, no external fonts/images.
- If requirements change, update the relevant doc in docs/ in the same task and list what changed.
