/**
 * ESP32 Smart Streetlight - Core Application Engine (Person A)
 * Implements docs/API_CONTRACT.md (v2.0)
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

    // Relay State (Read back from GPIO23)
    const isLightOn = Boolean(data.lightOn);
    el.relayStateBadge.textContent = isLightOn ? 'ON' : 'OFF';
    el.relayStateBadge.className = `relay-state-badge ${isLightOn ? 'badge-on' : 'badge-off'}`;

    // Automation Notice (shown strictly when automationEnabled = true)
    el.automationNotice.hidden = !Boolean(data.automationEnabled);

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
    const isAuto = state.lastStatus ? Boolean(state.lastStatus.automationEnabled) : false;
    const shouldDisable = !state.connected || isAuto || state.isCommandInFlight;
    el.btnTurnOn.disabled = shouldDisable;
    el.btnTurnOff.disabled = shouldDisable;
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

    if (state.lastStatus && state.lastStatus.automationEnabled) {
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
    const endpoint = (CONFIG.API_BASE || '') + '/api/light';

    try {
      const response = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Accept': 'application/json' },
        body: JSON.stringify({ state: targetState }),
        signal: controller.signal
      });
      clearTimeout(timeoutId);

      const data = await response.json().catch(() => ({}));
      if (response.ok && data.ok) {
        await pollStatus(); // Confirm real state from ESP32 readback
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

  el.btnTurnOn.addEventListener('click', () => sendLightCommand('ON'));
  el.btnTurnOff.addEventListener('click', () => sendLightCommand('OFF'));

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
