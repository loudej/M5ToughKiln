#ifndef FIRING_CONTROLLER_H
#define FIRING_CONTROLLER_H

#include <cstdint>
#include <string>
#include "../hardware/kiln_hardware.h"
#include "power_output.h"

class FiringProgram;

class FiringController {
private:
    IKilnHardware* hardware;
    PowerOutput*   powerOutput;

    // PID gains (Kp, Ki, Kd) live in `appState` and are editable from settings.

    uint32_t lastUpdateMs   = 0;
    float    errorSum       = 0.f;
    float    lastTemp       = 0.f;
    float    dFiltered      = 0.f;
    uint32_t lastPidMs      = 0;
    bool     pidHasPrevious = false;
    bool     pidTimeValid   = false;

    uint32_t programStartMs    = 0;  // millis() value when arm ran (always = arm-time now)
    uint32_t segmentStartMs    = 0;
    int      currentSegmentIdx = 0;
    float    segmentStartTemp  = 25.0f;
    /// Schedule seconds already elapsed at arm time (from alignedSetpointC alignment).
    /// totalTimeElapsed = armElapsedSec_ + (now - programStartMs) / kMsPerSecond.
    /// This decouples the display clock from millis() uptime so a reboot mid-firing
    /// still shows the correct elapsed time rather than near-zero.
    uint32_t armElapsedSec_    = 0;

    uint16_t consecutiveSensorFailures = 0;

    void enterErrorState(std::string message);

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
