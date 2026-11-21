/**
 * ESP32 Smart Streetlight - Isolated Demo Mode
 * Person A (Frontend)
 *
 * Implements requirement:
 * - OFF by default.
 * - Explicitly enabled via query string (?demo=1) or clicking "Demo Mode" button.
 * - Shows persistent banner: "DEMO MODE — SAMPLE DATA, NOT FROM HARDWARE"
 * - Separate code path; real mode never reads demo data.
 * - Provides switchable states (GPS Waiting, GPS Active, Automation ON).
 * - Allows testing button interaction against fake state.
 */

(function () {
  'use strict';

  // Demo Scenarios matching contract shapes
  const SCENARIOS = {
    gps_ok: {
      gpsValid: true,
      latitude: 12.9716,
      longitude: 77.5946,
      sunrise: '06:15',
      sunset: '17:55',
      lightOn: false,
      automationEnabled: false
    },
    gps_waiting: {
      gpsValid: false,
      latitude: null,
      longitude: null,
      sunrise: null,
      sunset: null,
      lightOn: false,
      automationEnabled: false
    },
    automation_on: {
      gpsValid: true,
      latitude: 12.9716,
      longitude: 77.5946,
      sunrise: '06:15',
      sunset: '17:55',
      lightOn: true,
      automationEnabled: true
    }
  };

  // Demo internal state
  let currentScenario = 'gps_ok';
  let demoLightState = false;

  // DOM Elements
  const demoBanner = document.getElementById('demo-banner');
  const scenarioSelect = document.getElementById('demo-scenario-select');
  const enterBtn = document.getElementById('demo-enter-btn');
  const exitBtn = document.getElementById('demo-exit-btn');
  const btnTurnOn = document.getElementById('btn-turn-on');
  const btnTurnOff = document.getElementById('btn-turn-off');

  /**
   * Activates Demo Mode.
   */
  function activateDemo() {
    if (!window.StreetlightApp) return;

    window.StreetlightApp.setDemoActive(true);
    demoBanner.hidden = false;
    document.body.classList.add('demo-active');
    enterBtn.hidden = true;

    // Load initial scenario
    loadScenario(scenarioSelect.value || 'gps_ok');
  }

  /**
   * Exits Demo Mode and returns to live hardware polling.
   */
  function deactivateDemo() {
    if (!window.StreetlightApp) return;

    demoBanner.hidden = true;
    document.body.classList.remove('demo-active');
    enterBtn.hidden = false;

    // Clean URL query parameter if present
    const url = new URL(window.location);
    if (url.searchParams.has('demo')) {
      url.searchParams.delete('demo');
      window.history.replaceState({}, '', url.pathname + url.search);
    }

    window.StreetlightApp.setDemoActive(false);
  }

  /**
   * Applies the selected mock scenario.
   */
  function loadScenario(key) {
    currentScenario = key;
    const base = SCENARIOS[key];
    if (!base) return;

    // Clone scenario to avoid mutations
    const mockData = Object.assign({}, base);
    mockData.lightOn = base.automationEnabled ? base.lightOn : demoLightState;

    window.StreetlightApp.setConnectionStatus(true);
    window.StreetlightApp.applyStatusData(mockData);
  }

  /**
   * Handles button clicks while in demo mode.
   */
  function handleDemoButtonClick(targetState) {
    const base = SCENARIOS[currentScenario];
    if (!base) return;

    if (base.automationEnabled) {
      window.StreetlightApp.setCommandError('Demo Notice: Manual control disabled because automation is on (409 Conflict).');
      return;
    }

    demoLightState = targetState === 'ON';
    window.StreetlightApp.setCommandError(null);
    loadScenario(currentScenario);
  }

  // Intercept button clicks in demo mode
  btnTurnOn.addEventListener('click', (e) => {
    if (document.body.classList.contains('demo-active')) {
      e.stopImmediatePropagation();
      handleDemoButtonClick('ON');
    }
  }, true);

  btnTurnOff.addEventListener('click', (e) => {
    if (document.body.classList.contains('demo-active')) {
      e.stopImmediatePropagation();
      handleDemoButtonClick('OFF');
    }
  }, true);

  // Scenario dropdown change
  scenarioSelect.addEventListener('change', (e) => {
    loadScenario(e.target.value);
  });

  // UI trigger buttons
  enterBtn.addEventListener('click', activateDemo);
  exitBtn.addEventListener('click', deactivateDemo);

  // Auto-activate if URL contains ?demo=1 or ?demo=true
  document.addEventListener('DOMContentLoaded', () => {
    const params = new URLSearchParams(window.location.search);
    if (params.get('demo') === '1' || params.get('demo') === 'true') {
      activateDemo();
    }
  });

})();
