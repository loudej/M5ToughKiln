#include "power_output.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <cmath>
#include <algorithm>

void PowerOutput::setEnabled(bool enabled) {
    if (enabled == _enabled) return;

    if (!enabled) {
        const uint32_t now = millis();
        M5.Log.printf("PowerOutput disabled after %u ms (accumulator=%.3f)\n",
                      _lastTransitionMs ? (now - _lastTransitionMs) : 0u,
                      _accumulator);
        _enabled        = false;
        _power          = 0.0f;
        _bias           = 1.0f;
        _effectiveDwell = MIN_DWELL_MS;
        _accumulator    = 0.0f;
        _appliedOn    = false;
        _lastTransitionMs = now;
        _hardware->setRelay(false);
        return;
    }

    const uint32_t now = millis();
    _enabled          = true;
    _power            = 0.0f;
    _bias             = 1.0f;
    _effectiveDwell   = MIN_DWELL_MS;
    _accumulator      = 0.0f;
    _appliedOn        = false;
    _lastUpdateMs     = now;
    _lastTransitionMs = now;
}

void PowerOutput::setPower(float power) {
    if (!_enabled) return;
    if (power < 0.0f) power = 0.0f;
    if (power > 1.0f) power = 1.0f;
    _power         = power;
    _bias          = 1.0f - 2.0f * power;
    _effectiveDwell = static_cast<uint32_t>(
        std::max(static_cast<float>(MIN_DWELL_MS),
                 static_cast<float>(WINDOW_MS) * std::min(power, 1.0f - power)));
}

void PowerOutput::update() {
    if (!_enabled) {
        _hardware->setRelay(false);
        _appliedOn = false;
        return;
    }

    const uint32_t now   = millis();
    const float    dtSec = static_cast<float>(now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    // Accumulator has two independent contributions each tick:
    //   Relay state: +1/s when ON, -1/s when OFF
    //   Demand bias: _bias/s  →  0% power = +1/s, 50% = 0, 100% = -1/s
    // Combined, drift is zero at steady-state duty = power for any power level.
    _accumulator += _bias * dtSec;
    if (_appliedOn) {
        _accumulator += dtSec;
    } else {
        _accumulator -= dtSec;
    }

    // Check if the accumulator wants a transition.
    const bool wantOff = _appliedOn  && (_accumulator > 0.0f);
    const bool wantOn  = !_appliedOn && (_accumulator < 0.0f);
    if (!wantOff && !wantOn) return;

    const uint32_t timeInState = now - _lastTransitionMs;
    if (timeInState < _effectiveDwell) return;

    // Transition.
    _appliedOn        = wantOn;
    _lastTransitionMs = now;
    _hardware->setRelay(_appliedOn);

    M5.Log.printf("Relay %s after %u ms (power=%.2f accumulator=%.3f dwell=%u ms)\n",
                  _appliedOn ? "ON" : "OFF",
                  timeInState,
                  _power,
                  _accumulator,
                  _effectiveDwell);
}
