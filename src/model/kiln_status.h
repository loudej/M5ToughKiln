#ifndef KILN_STATUS_H
#define KILN_STATUS_H

#include <cstdint>

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
    uint32_t segmentTimeElapsed = 0;
    uint32_t totalTimeElapsed = 0;
    const char* activeProgramName = "None";
};

#endif // KILN_STATUS_H
