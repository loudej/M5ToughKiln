#ifndef FIRING_CONTROLLER_H
#define FIRING_CONTROLLER_H

#include "../model/app_state.h"
#include "../hardware/kiln_hardware.h"

class FiringController {
private:
    IKilnHardware* hardware;
    
    // PID state
    float errorSum = 0;
    float lastError = 0;
    float kp = 2.0f;
    float ki = 0.01f;
    float kd = 10.0f;

    // Time proportioning window (60 seconds)
    const uint32_t windowSizeMs = 60000;
    uint32_t windowStartMs = 0;
    uint32_t relayOnTimeMs = 0; // How long the relay should be ON this window

    // State machine tracking
    uint32_t lastUpdateMs = 0;
    uint32_t programStartMs = 0;
    uint32_t segmentStartMs = 0;
    int currentSegmentIdx = 0;
    float segmentStartTemp = 25.0f;

    void processSegment();
    void updatePID(float setpoint, float currentTemp);

public:
    FiringController(IKilnHardware* hw);
    
    // Called continuously in loop()
    void update();
};

#endif // FIRING_CONTROLLER_H
