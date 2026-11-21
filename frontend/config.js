/**
 * ESP32 Smart Streetlight - Frontend Configuration
 * Person A (Frontend)
 *
 * This configuration controls API endpoints and polling parameters.
 * When served directly from ESP32 LittleFS, API_BASE should remain "" (relative paths).
 * For local frontend development targeting a physical ESP32, set API_BASE to "http://<ESP32_IP>".
 */
const CONFIG = {
  // Base URL for API requests. Empty string "" means relative to current host.
  API_BASE: '',

  // Polling interval for GET /api/status in milliseconds (Contract: ~1500 ms)
  POLL_INTERVAL_MS: 1500,

  // Request timeout in milliseconds (Contract: ~3000 ms)
  REQUEST_TIMEOUT_MS: 3000,

  // Number of consecutive network failures before marking ESP32 as Disconnected
  MAX_CONSECUTIVE_FAILURES: 3
};

// Freeze configuration to prevent accidental modification at runtime
Object.freeze(CONFIG);
