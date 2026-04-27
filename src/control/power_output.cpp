#include "power_output.h"
#include <M5Unified.h>
#include <Arduino.h>

void PowerOutput::setEnabled(bool enabled) {
    if (enabled == _enabled) return;
    if (!enabled) {
        if (_enabled) {
            const uint32_t now = millis();
            const uint32_t x   = _lastTransitionMs ? (now - _lastTransitionMs) : 0U;
            M5.Log.printf("Relay OFF after %u ms for 0 ms (disabled)\n", x);
        }
        _enabled = false;
        _power = 0.0f;
        _relayOnMs = 0;
        _windowOnMs = 0;
        _appliedOn = false;
        _lastTransitionMs = millis();
        _hardware->setRelay(false);
        return;
    }
    _enabled = true;
    _power = 0.0f;
    _relayOnMs = 0;
    _windowOnMs = 0;
    _appliedOn = false;
    _windowStartMs = millis();
    _lastTransitionMs = _windowStartMs;
}

void PowerOutput::setPower(float power) {
    if (!_enabled) return;
    if (power < 0.0f) power = 0.0f;
    if (power > 1.0f) power = 1.0f;
    _power = power;
    _relayOnMs = (uint32_t)(_power * WINDOW_MS);
}

void PowerOutput::update() {
    if (!_enabled) {
        _hardware->setRelay(false);
        _appliedOn = false;
        return;
    }

    uint32_t now = millis();
    uint32_t elapsed = now - _windowStartMs;
    if (elapsed >= WINDOW_MS) {
        _windowStartMs = now;
        elapsed = 0;
    }

    //uint32_t wOn = _relayOnMs;
    //if (wOn > 0 && wOn < MIN_DWELL_MS) wOn = MIN_DWELL_MS;
    //if (wOn > (WINDOW_MS - MIN_DWELL_MS) && wOn < WINDOW_MS) wOn = WINDOW_MS - MIN_DWELL_MS;
    _windowOnMs = _relayOnMs;

    const bool shouldBeOn = (_windowOnMs > 0) && (elapsed < _windowOnMs);

    if (shouldBeOn == _appliedOn) {
        return;
    }

    const uint32_t actualTimeInCurrentState = now - _lastTransitionMs;
    if (actualTimeInCurrentState < MIN_DWELL_MS) {
        return;
    }

    const uint32_t expectedTimeInNewState =
        shouldBeOn ? ((_windowOnMs > elapsed) ? (_windowOnMs - elapsed) : 0U)
                   : ((WINDOW_MS > elapsed) ? (WINDOW_MS - elapsed) : 0U);
    if (expectedTimeInNewState < MIN_DWELL_MS) {
        return;
    }

    M5.Log.printf("Relay %s after %u ms for %u ms\n",
                  shouldBeOn ? "ON" : "OFF", actualTimeInCurrentState, expectedTimeInNewState);
    _hardware->setRelay(shouldBeOn);
    _appliedOn = shouldBeOn;
    _lastTransitionMs = now;
}
