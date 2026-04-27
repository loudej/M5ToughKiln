#ifndef FIRING_CONTROLLER_H
#define FIRING_CONTROLLER_H

#include <cstdint>
#include "../hardware/kiln_hardware.h"
#include "power_output.h"

class FiringController {
private:
    IKilnHardware* hardware;
    PowerOutput*   powerOutput;

    // Discrete-time PID: output 0–100 (%). Ki integrates ∑(e·dt) in °C·s.
    // Kd uses derivative-on-measurement: −d(measured temp)/dt (°C/s), avoids
    // derivative kick on setpoint steps. Retune on hardware as needed.
    float kp = 0.5f;
    float ki = 0.015f;
    float kd = 0.4f;

    uint32_t lastUpdateMs   = 0;
    float    errorSum       = 0.f;
    float    lastTemp       = 0.f;
    float    dFiltered      = 0.f;
    uint32_t lastPidMs      = 0;
    bool     pidHasPrevious = false;
    bool     pidTimeValid   = false;

    uint32_t programStartMs    = 0;
    uint32_t segmentStartMs    = 0;
    int      currentSegmentIdx = 0;
    float    segmentStartTemp  = 25.0f;

    void armPowerIfNeeded(uint32_t now);
    void applyTelemetryAndPid(uint32_t now, float setpoint);
    void processSegment(uint32_t now);
    void updatePID(float setpoint, float currentTemp, uint32_t now);

public:
    FiringController(IKilnHardware* hw, PowerOutput* po);

    // Called continuously in loop(). uint32_t time math is safe across millis()
    // rollover for intervals under ~49 days (unsigned wrap semantics).
    void update();
};

#endif // FIRING_CONTROLLER_H
