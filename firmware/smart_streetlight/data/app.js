/**
 * ESP32 Smart Streetlight - Core Application Engine (SolarSync)
 * Implements docs/API_CONTRACT.md (v2.1)
 */
(function () {
  'use strict';

  const state = {
    connected: false,
    consecutiveFailures: 0,
    pollTimer: null,
    isPollInFlight: false,
    isCommandInFlight: false,
    activeAbortController: null,
    lastStatus: null,
    isDemoActive: false
  };

  const el = {
    connectionPill: document.getElementById('connection-pill'),
    connectionText: document.getElementById('connection-text'),
    valLatitude: document.getElementById('val-latitude'),
    valLongitude: document.getElementById('val-longitude'),
    valSunrise: document.getElementById('val-sunrise'),
    valSunset: document.getElementById('val-sunset'),
    btnModeAuto: document.getElementById('btn-mode-auto'),
    btnModeManual: document.getElementById('btn-mode-manual'),
    relayStateBadge: document.getElementById('relay-state-badge'),
    automationNotice: document.getElementById('automation-notice'),
    btnTurnOn: document.getElementById('btn-turn-on'),
    btnTurnOff: document.getElementById('btn-turn-off'),
    commandError: document.getElementById('command-error')
  };

  function applyStatusData(data) {
    state.lastStatus = data;

    // Latitude & Longitude
    el.valLatitude.textContent = (data.gpsValid && data.latitude !== null && data.latitude !== undefined)
      ? Number(data.latitude).toFixed(4) : '--';
    el.valLongitude.textContent = (data.gpsValid && data.longitude !== null && data.longitude !== undefined)
      ? Number(data.longitude).toFixed(4) : '--';

    // Sunrise & Sunset
    el.valSunrise.textContent = data.sunrise || '--';
    el.valSunset.textContent = data.sunset || '--';

    // Mode determination
    const isAuto = data.mode ? (data.mode === 'AUTO') : Boolean(data.automationEnabled);

    if (el.btnModeAuto && el.btnModeManual) {
      el.btnModeAuto.classList.toggle('active', isAuto);
      el.btnModeAuto.setAttribute('aria-checked', isAuto ? 'true' : 'false');
      el.btnModeManual.classList.toggle('active', !isAuto);
      el.btnModeManual.setAttribute('aria-checked', !isAuto ? 'true' : 'false');
    }

    // Automation Notice (shown strictly when AUTO mode is active)
    el.automationNotice.hidden = !isAuto;

    // Relay State (Read back from ESP32 GPIO23)
    const isLightOn = Boolean(data.lightOn);
    el.relayStateBadge.textContent = isLightOn ? 'ON' : 'OFF';
    el.relayStateBadge.className = `relay-state-badge ${isLightOn ? 'badge-on' : 'badge-off'}`;

    updateButtonStates();
  }

  function resetLiveTelemetryToDefault() {
    el.valLatitude.textContent = '--';
    el.valLongitude.textContent = '--';
    el.valSunrise.textContent = '--';
    el.valSunset.textContent = '--';
    el.relayStateBadge.textContent = '--';
    el.relayStateBadge.className = 'relay-state-badge badge-off';
    el.automationNotice.hidden = true;
    updateButtonStates();
  }

  function setConnectionStatus(isConnected) {
    state.connected = isConnected;
    el.connectionPill.className = `connection-pill ${isConnected ? 'connected' : 'disconnected'}`;
    el.connectionText.textContent = isConnected ? 'Connected' : 'Disconnected';
    updateButtonStates();
  }

  function updateButtonStates() {
    const isAuto = state.lastStatus
      ? (state.lastStatus.mode ? (state.lastStatus.mode === 'AUTO') : Boolean(state.lastStatus.automationEnabled))
      : false;

    // When AUTO: manual ON/OFF buttons are disabled
    // When MANUAL: manual ON/OFF buttons are enabled
    const shouldDisableControls = !state.connected || isAuto || state.isCommandInFlight;
    el.btnTurnOn.disabled = shouldDisableControls;
    el.btnTurnOff.disabled = shouldDisableControls;

    // Mode buttons
    const shouldDisableMode = !state.connected || state.isCommandInFlight;
    if (el.btnModeAuto) el.btnModeAuto.disabled = shouldDisableMode;
    if (el.btnModeManual) el.btnModeManual.disabled = shouldDisableMode;
  }

  function setCommandError(message) {
    if (message) {
      el.commandError.textContent = message;
      el.commandError.hidden = false;
    } else {
      el.commandError.textContent = '';
      el.commandError.hidden = true;
    }
  }

  async function pollStatus() {
    if (state.isDemoActive || state.isPollInFlight) return;

    state.isPollInFlight = true;
    const controller = new AbortController();
    state.activeAbortController = controller;

    const timeoutId = setTimeout(() => controller.abort(), CONFIG.REQUEST_TIMEOUT_MS);
    const endpoint = (CONFIG.API_BASE || '') + '/api/status';

    try {
      const response = await fetch(endpoint, {
        method: 'GET',
        headers: { 'Accept': 'application/json' },
        signal: controller.signal
      });
      clearTimeout(timeoutId);

      if (!response.ok) throw new Error(`HTTP error ${response.status}`);
      const data = await response.json();

      state.consecutiveFailures = 0;
      if (!state.connected) setConnectionStatus(true);
      applyStatusData(data);
    } catch (err) {
      clearTimeout(timeoutId);
      state.consecutiveFailures += 1;
      if (state.consecutiveFailures >= CONFIG.MAX_CONSECUTIVE_FAILURES) {
        if (state.connected || state.consecutiveFailures === CONFIG.MAX_CONSECUTIVE_FAILURES) {
          setConnectionStatus(false);
          resetLiveTelemetryToDefault();
        }
      }
    } finally {
      state.isPollInFlight = false;
      state.activeAbortController = null;
    }
  }

  function startPolling() {
    if (state.pollTimer) clearInterval(state.pollTimer);
    pollStatus();
    state.pollTimer = setInterval(pollStatus, CONFIG.POLL_INTERVAL_MS);
  }

  function stopPolling() {
    if (state.pollTimer) {
      clearInterval(state.pollTimer);
      state.pollTimer = null;
    }
    if (state.activeAbortController) {
      state.activeAbortController.abort();
    }
  }

  async function sendLightCommand(targetState) {
    if (state.isDemoActive || !state.connected || state.isCommandInFlight) return;

    const isAuto = state.lastStatus
      ? (state.lastStatus.mode ? (state.lastStatus.mode === 'AUTO') : Boolean(state.lastStatus.automationEnabled))
      : false;

    if (isAuto) {
      setCommandError('Manual control is disabled while automation is enabled.');
      return;
    }

    state.isCommandInFlight = true;
    setCommandError(null);
    updateButtonStates();

    const activeBtn = targetState === 'ON' ? el.btnTurnOn : el.btnTurnOff;
    const spinner = activeBtn.querySelector('.btn-spinner');
    if (spinner) spinner.hidden = false;

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), CONFIG.REQUEST_TIMEOUT_MS);
    
    // Per requirement: Turn ON calls GET /api/light/on, Turn OFF calls GET /api/light/off
    const endpoint = (CONFIG.API_BASE || '') + (targetState === 'ON' ? '/api/light/on' : '/api/light/off');

    try {
      const response = await fetch(endpoint, {
        method: 'GET',
        headers: { 'Accept': 'application/json' },
        signal: controller.signal
      });
      clearTimeout(timeoutId);

      const data = await response.json().catch(() => ({}));
      if (response.ok && data.ok) {
        // Fetch /api/status and update the Relay Output status immediately
        await pollStatus();
      } else {
        const errMsg = data.message || `Command failed with status ${response.status} (${data.error || 'ERROR'})`;
        setCommandError(errMsg);
      }
    } catch (err) {
      clearTimeout(timeoutId);
      setCommandError(err.name === 'AbortError' ? 'Command timed out.' : 'Network error reaching ESP32.');
    } finally {
      if (spinner) spinner.hidden = true;
      state.isCommandInFlight = false;
      updateButtonStates();
    }
  }

  async function switchMode(targetMode) {
    if (state.isDemoActive || !state.connected || state.isCommandInFlight) return;

    state.isCommandInFlight = true;
    setCommandError(null);
    updateButtonStates();

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), CONFIG.REQUEST_TIMEOUT_MS);
    const endpoint = (CONFIG.API_BASE || '') + (targetMode === 'AUTO' ? '/api/mode/auto' : '/api/mode/manual');

    try {
      const response = await fetch(endpoint, {
        method: 'GET',
        headers: { 'Accept': 'application/json' },
        signal: controller.signal
      });
      clearTimeout(timeoutId);

      const data = await response.json().catch(() => ({}));
      if (response.ok && data.ok) {
        await pollStatus();
      } else {
        setCommandError(data.message || `Mode switch failed (${data.error || 'ERROR'})`);
      }
    } catch (err) {
      clearTimeout(timeoutId);
      setCommandError(err.name === 'AbortError' ? 'Mode switch timed out.' : 'Network error reaching ESP32.');
    } finally {
      state.isCommandInFlight = false;
      updateButtonStates();
    }
  }

  el.btnTurnOn.addEventListener('click', () => sendLightCommand('ON'));
  el.btnTurnOff.addEventListener('click', () => sendLightCommand('OFF'));

  if (el.btnModeAuto) el.btnModeAuto.addEventListener('click', () => switchMode('AUTO'));
  if (el.btnModeManual) el.btnModeManual.addEventListener('click', () => switchMode('MANUAL'));

  window.StreetlightApp = {
    applyStatusData,
    setConnectionStatus,
    resetLiveTelemetryToDefault,
    setCommandError,
    startPolling,
    stopPolling,
    setDemoActive: (active) => {
      state.isDemoActive = active;
      if (active) {
        stopPolling();
      } else {
        setCommandError(null);
        startPolling();
      }
    }
  };

  document.addEventListener('DOMContentLoaded', () => {
    if (!state.isDemoActive) startPolling();
  });
})();
