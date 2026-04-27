#ifndef POWER_OUTPUT_H
#define POWER_OUTPUT_H

#include <cstdint>
#include "../hardware/kiln_hardware.h"

// Drives the relay/contactor through the IKilnHardware interface using a
// fixed-period time-proportioning scheme.
//
// Usage contract:
//   setEnabled(true)   — arms the output; resets power to 0.0
//   setPower(0.0–1.0)  — sets desired effort; has no effect while disabled
//   update()           — call every loop iteration to clock the relay
//   setEnabled(false)  — disarms output, forces relay off, resets power to 0.0
//
// Power is only reset when enabled *changes*, so calling setEnabled(true)
// repeatedly while already enabled is a no-op and will not disturb the
// current power level.
//
// Ideal ON/OFF follows live PID demand each frame (not latched for the whole
// window). Actual GPIO transitions are rate-limited: at most one change every
// MIN_DWELL_MS, and only when the upcoming state would also last at least
// MIN_DWELL_MS within the current window schedule.
class PowerOutput {
public:
    // Window period for time-proportioning. At power=0.1, relay is ON for
    // 10% of this window and OFF for 90%.
    static constexpr uint32_t WINDOW_MS    = 10000;

    // Minimum on/off dwell between *actual* relay transitions and minimum
    // scheduled time remaining in the new state before a transition is allowed.
    // ⚠️  No datasheet is available for the Baomain HC1-50; this value is a
    // conservative placeholder. Verify against manufacturer specs before use.
    static constexpr uint32_t MIN_DWELL_MS = 500;

    explicit PowerOutput(IKilnHardware* hw) : _hardware(hw) {}

    void setEnabled(bool enabled);
    bool getEnabled() const { return _enabled; }

    // 0.0 = fully off, 1.0 = fully on. Clamped to [0, 1]. No-op if disabled.
    void setPower(float power);
    float getPower() const { return _power; }

    // Must be called every loop iteration while the output is active.
    void update();

private:
    IKilnHardware* _hardware;

    bool     _enabled           = false;
    float    _power             = 0.0f;
    uint32_t _relayOnMs         = 0;  // raw demand from PID (ms ON per window)
    uint32_t _windowOnMs        = 0;  // clamped demand (same frame as shouldBeOn)
    uint32_t _windowStartMs     = 0;
    bool     _appliedOn         = false;  // last value passed to setRelay
    uint32_t _lastTransitionMs  = 0;      // millis() at last applied relay edge
};

#endif // POWER_OUTPUT_H
