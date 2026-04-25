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
    uint32_t segmentTimeElapsed = 0;  // minutes — compared against soakTime
    uint32_t totalTimeElapsed   = 0;  // seconds — used by UI for elapsed/remaining/bar
    std::string activeProgramName = "None";
};

#endif // KILN_STATUS_H
