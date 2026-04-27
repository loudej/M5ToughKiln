#ifndef KILN_STATUS_H
#define KILN_STATUS_H

#include <cstdint>
#include <string>

#include "../hardware/kiln_sensor_read.h"

enum class KilnState {
    IDLE,
    RAMPING,
    SOAKING,
    COOLING,
    /// Program finished normally; heat off; clock still runs until operator taps DONE.
    DONE,
    ERROR
};

struct KilnStatus {
    KilnState currentState = KilnState::IDLE;
    float currentTemperature = 25.0f;
    /// Last `readSensor()` snapshot (copy of what `IKilnHardware` returned).
    KilnSensorRead sensor{};
    float targetTemperature = 25.0f;

    /// Peak segment target (°C) for the active firing — set when the run arms.
    float programPeakTemperatureC = 0.f;
    /// Measured kiln temp (°C) when START armed — used with 75°F floor for thermal bar range.
    float programRunStartTemperatureC = 0.f;
    // Segment soak timer: minutes elapsed in the current segment (since
    // segmentStartMs). Must stay consistent with FiringSegment::soakTime (minutes).
    uint32_t segmentTimeElapsed = 0;
    uint32_t totalTimeElapsed   = 0;  // seconds — used by UI for elapsed/remaining/bar
    float    power              = 0.0f;  // 0.0–1.0, current PID output effort
    std::string activeProgramName = "None";

    /// Set once when the firing controller enters ERROR; cleared on IDLE. Live sensor
    /// text may differ — this is the operator-facing reason shown until RESET.
    std::string frozenControllerError{};
};

/// JSON string for `kilnState` on web/API (`idle`, `ramping`, …).
inline const char* kilnStateJsonKey(KilnState s) {
    switch (s) {
        case KilnState::IDLE:
            return "idle";
        case KilnState::RAMPING:
            return "ramping";
        case KilnState::SOAKING:
            return "soaking";
        case KilnState::COOLING:
            return "cooling";
        case KilnState::DONE:
            return "done";
        case KilnState::ERROR:
            return "error";
        default:
            return "idle";
    }
}

#endif // KILN_STATUS_H
