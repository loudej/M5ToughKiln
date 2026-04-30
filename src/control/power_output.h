#ifndef POWER_OUTPUT_H
#define POWER_OUTPUT_H

#include <cstdint>
#include "../hardware/kiln_hardware.h"

// Drives the relay/contactor through the IKilnHardware interface using a
// sigma-delta (error-diffusion) scheme.
//
// Algorithm:
//   An accumulator tracks the difference between delivered and demanded energy.
//   Each update tick the accumulator drifts based on relay state and demand:
//     relay ON  → accumulator += 2(1 - power) × dt  (delivering energy, pulling toward OFF)
//     relay OFF → accumulator -= 2 × power × dt      (not delivering, pulling toward ON)
//   At any power level p the accumulator has zero average drift when duty = p,
//   so the relay fires at exactly the demanded proportion with no dead band.
//
//   Switching is gated by an effective minimum dwell that scales with demand:
//     effectiveDwell = max(MIN_DWELL_MS, WINDOW_MS × min(power, 1 − power))
//   This gives a 10 s period (like PWM) above 20% / below 80%, and below that
//   pins the shorter half-wave at MIN_DWELL_MS while stretching the other half
//   to maintain the correct ratio.
//
// Usage contract:
//   setEnabled(true)   — arms the output; resets accumulator and power to 0
//   setPower(0.0–1.0)  — sets desired effort; no-op while disabled
//   update()           — call every loop iteration (~100 ms)
//   setEnabled(false)  — disarms output, forces relay off
class PowerOutput {
public:
    // Reference window used to scale the minimum dwell. At 50% demand the
    // effective dwell is WINDOW_MS / 2, giving a natural period of WINDOW_MS.
    static constexpr uint32_t WINDOW_MS   = 10000;

    // Absolute floor for the effective minimum dwell. Prevents relay chattering
    // at very low or very high demand levels.
    // ⚠️  No datasheet is available for the Baomain HC1-50; verify against
    // manufacturer specs before reducing below the current value.
    static constexpr uint32_t MIN_DWELL_MS = 2000;

    explicit PowerOutput(IKilnHardware* hw) : _hardware(hw) {}

    void  setEnabled(bool enabled);
    bool  getEnabled() const { return _enabled; }

    // 0.0 = fully off, 1.0 = fully on. Clamped to [0, 1]. No-op if disabled.
    void  setPower(float power);
    float getPower() const { return _power; }

    // Must be called every loop iteration while the output is active.
    void update();

private:
    IKilnHardware* _hardware;

    bool     _enabled          = false;
    float    _power            = 0.0f;

    // Sigma-delta state. Positive → relay has been ON more than demanded (bias toward OFF).
    // Negative → relay has been OFF more than demanded (bias toward ON).
    float    _accumulator      = 0.0f;
    // Demand bias in accumulator-units/sec. Set alongside _power: (1 - 2×power).
    // 0% power → +1.0/s (always wants ON), 100% → -1.0/s (always wants OFF), 50% → 0.
    float    _bias             = 1.0f;   // default matches _power = 0.0
    // Effective minimum dwell (ms). Set alongside _power:
    //   max(MIN_DWELL_MS, WINDOW_MS × min(power, 1 − power))
    uint32_t _effectiveDwell   = MIN_DWELL_MS;

    uint32_t _lastUpdateMs     = 0;   // millis() of previous update() call
    bool     _appliedOn        = false;
    uint32_t _lastTransitionMs = 0;   // millis() of last actual relay edge
};

#endif // POWER_OUTPUT_H
