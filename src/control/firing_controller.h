#ifndef FIRING_CONTROLLER_H
#define FIRING_CONTROLLER_H

#include "../hardware/kiln_hardware.h"
#include "power_output.h"

class FiringController {
private:
    IKilnHardware* hardware;
    PowerOutput*   powerOutput;

    // PID state
    float errorSum  = 0;
    float lastError = 0;
    float kp = 2.0f;
    float ki = 0.01f;
    float kd = 10.0f;

    // State machine tracking
    uint32_t programStartMs  = 0;
    uint32_t segmentStartMs  = 0;
    int      currentSegmentIdx = 0;
    float    segmentStartTemp  = 25.0f;

    void processSegment();
    void updatePID(float setpoint, float currentTemp);

public:
    FiringController(IKilnHardware* hw, PowerOutput* po);

    // Called continuously in loop()
    void update();
};

#endif // FIRING_CONTROLLER_H
