#ifndef KILN_STATUS_H
#define KILN_STATUS_H

#include <cstdint>
#include <string>

enum class KilnState {
    IDLE,
    RAMPING,
    SOAKING,
    COOLING,
    ERROR
};

struct KilnStatus {
    KilnState currentState = KilnState::IDLE;
    float currentTemperature = 25.0f;
    float targetTemperature = 25.0f;
    // Segment soak timer: minutes elapsed in the current segment (since
    // segmentStartMs). Must stay consistent with FiringSegment::soakTime (minutes).
    uint32_t segmentTimeElapsed = 0;
    uint32_t totalTimeElapsed   = 0;  // seconds — used by UI for elapsed/remaining/bar
    float    power              = 0.0f;  // 0.0–1.0, current PID output effort
    std::string activeProgramName = "None";
};

#endif // KILN_STATUS_H
