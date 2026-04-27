#include "firing_controller.h"
#include "../hardware/kiln_sensor_read.h"
#include "../model/app_state.h"
#include "../model/firing_program.h"
#include <Arduino.h>
#include <string>

namespace {

constexpr uint32_t kMsPerSecond = 1000;
constexpr uint32_t kMsPerMinute = 60000;
constexpr uint32_t kMsPerHour   = 3600000;

constexpr uint32_t kUpdateIntervalMs = 100;

constexpr float kPidOutMin = 0.f;
constexpr float kPidOutMax = 100.f;

constexpr float kIntegralErrorBand_C = 50.f;
constexpr float kIntegralCap         = 500.f;  // cap on |∫e·dt| (°C·s scale)

// First PID step after arm / reset has no dt sample; assume ~50 ms until lastPidMs is valid.
constexpr float kInitialPidDt_sec = 0.05f;
constexpr float kMaxPidDt_sec     = 2.f;
constexpr float kMinPidDt_sec     = 0.001f;

constexpr float kDerivativeLpfAlpha = 0.25f;

constexpr uint16_t kMaxConsecutiveSensorFailures = 25;  // ~2.5 s at 100 ms cadence

/// Ramp phase is “done” by temperature (or absent when rampRate == 0).
bool rampGoalReached(const FiringSegment& seg, float from, float to, float cur) {
    if (seg.rampRate <= 0.f)
        return true;
    if (from < to)
        return cur >= to;
    if (from > to)
        return cur <= to;
    return true;
}

}  // namespace

void FiringController::enterErrorState(std::string message) {
    if (appState.status_.currentState == KilnState::ERROR)
        return;
    appState.status_.currentState           = KilnState::ERROR;
    appState.status_.frozenControllerError = std::move(message);
}

void FiringController::setStateForActiveRampPhase(float from, float to) {
    if (from < to)
        appState.status_.currentState = KilnState::RAMPING;
    else if (from > to)
        appState.status_.currentState = KilnState::COOLING;
    else
        appState.status_.currentState = KilnState::RAMPING;
}

FiringController::FiringController(IKilnHardware* hw, PowerOutput* po)
    : hardware(hw), powerOutput(po) {}

void FiringController::update() {
    const uint32_t now = millis();

    if (lastUpdateMs != 0) {
        const uint32_t dt = now - lastUpdateMs;
        if (dt >= kUpdateIntervalMs + 10) {
            Serial.printf("[FiringController] update cadence late: dt=%lu ms (nominal %lu ms)\n",
                          static_cast<unsigned long>(dt),
                          static_cast<unsigned long>(kUpdateIntervalMs));
        }
    }

    lastUpdateMs = now;

    appState.status_.sensor             = hardware->readSensor();
    appState.status_.currentTemperature = appState.status_.sensor.thermocoupleCelsius;

    if (appState.status_.currentState == KilnState::IDLE || appState.status_.currentState == KilnState::ERROR) {
        consecutiveSensorFailures = 0;
        if (appState.status_.currentState == KilnState::IDLE)
            appState.status_.frozenControllerError.clear();
        appState.status_.programPeakTemperatureC     = 0.f;
        appState.status_.programRunStartTemperatureC = 0.f;
        powerOutput->setEnabled(false);
        appState.status_.power = 0.f;
        segmentStartMs    = 0;
        programStartMs    = 0;
        currentSegmentIdx = 0;
        appState.status_.totalTimeElapsed   = 0;
        appState.status_.segmentTimeElapsed = 0;
        lastPidMs       = 0;
        pidHasPrevious  = false;
        pidTimeValid    = false;
        dFiltered       = 0.f;
        lastTemp        = 0.f;
        return;
    }

    if (appState.status_.currentState == KilnState::DONE) {
        consecutiveSensorFailures = 0;
        appState.status_.totalTimeElapsed = (now - programStartMs) / kMsPerSecond;
        appState.status_.power            = 0.f;
        powerOutput->setPower(0.f);
        powerOutput->setEnabled(false);
        powerOutput->update();
        return;
    }

    if (!appState.status_.sensor.controlUsable()) {
        consecutiveSensorFailures++;
        if (consecutiveSensorFailures >= kMaxConsecutiveSensorFailures) {
            const KilnSensorRead& sr = appState.status_.sensor;
            if (sr.deviceReportsFault()) {
                char detail[32];
                kilnFormatStatusFaultLine(sr.statusRegister, detail, sizeof detail);
                this->enterErrorState("ERROR: "+std::string(detail));
            } else {
                this->enterErrorState("ERROR: Sensor lost");
            }
        }
        powerOutput->setPower(0.f);
        powerOutput->update();
        return;
    }
    consecutiveSensorFailures = 0;

    armPowerIfNeeded(now);
    processSegment(now);
    powerOutput->update();
}

void FiringController::armPowerIfNeeded(uint32_t now) {
    if (powerOutput->getEnabled()) return;

    powerOutput->setEnabled(true);
    programStartMs   = now;
    segmentStartMs   = now;
    segmentStartTemp = appState.status_.currentTemperature;

    appState.status_.programRunStartTemperatureC = appState.status_.currentTemperature;
    appState.status_.programPeakTemperatureC     = 0.f;
    if (FiringProgram* ap = appState.mutableActiveProgram()) {
        for (const auto& s : ap->segments) {
            if (s.targetTemperature > appState.status_.programPeakTemperatureC)
                appState.status_.programPeakTemperatureC = s.targetTemperature;
        }
    }

    errorSum         = 0.f;
    lastTemp         = appState.status_.currentTemperature;
    dFiltered        = 0.f;
    lastPidMs        = 0;
    pidHasPrevious   = false;
    // No dt sample yet; first updatePID uses kInitialPidDt_sec until pidTimeValid is set.
    pidTimeValid     = false;
}

void FiringController::fastForwardCompletedRampPhases(uint32_t now, FiringProgram* prog) {
    const float cur = appState.status_.currentTemperature;

    for (;;) {
        if (currentSegmentIdx >= (int)prog->segments.size())
            return;

        FiringSegment& seg    = prog->segments[currentSegmentIdx];
        const float      from = segmentStartTemp;
        const float      to   = seg.targetTemperature;

        if (!rampGoalReached(seg, from, to, cur)) {
            setStateForActiveRampPhase(from, to);
            return;
        }

        currentSegmentIdx++;
        segmentStartTemp                   = to;
        segmentStartMs                     = now;
        appState.status_.segmentTimeElapsed = 0;
    }
}

void FiringController::applyTelemetryAndPid(uint32_t now, float setpoint) {
    appState.status_.segmentTimeElapsed = (now - segmentStartMs) / kMsPerMinute;
    appState.status_.totalTimeElapsed   = (now - programStartMs) / kMsPerSecond;
    appState.status_.targetTemperature  = setpoint;
    updatePID(setpoint, appState.status_.currentTemperature, now);
}

void FiringController::processSegment(uint32_t now) {
    FiringProgram* prog = appState.mutableActiveProgram();
    if (!prog) {
        this->enterErrorState("No program");
        return;
    }

    if (appState.status_.currentState == KilnState::RAMPING ||
        appState.status_.currentState == KilnState::COOLING) {
        fastForwardCompletedRampPhases(now, prog);
    }

    if (currentSegmentIdx >= (int)prog->segments.size()) {
        appState.status_.currentState = KilnState::DONE;
        return;
    }

    FiringSegment& seg = prog->segments[currentSegmentIdx];
    float setpoint = segmentStartTemp;

    if (appState.status_.currentState == KilnState::RAMPING ||
        appState.status_.currentState == KilnState::COOLING) {
        if (seg.rampRate <= 0.f) {
            appState.status_.currentState = KilnState::SOAKING;
            segmentStartMs               = now;
            setpoint                     = seg.targetTemperature;
            applyTelemetryAndPid(now, setpoint);
            return;
        }

        const float hoursElapsed = (now - segmentStartMs) / static_cast<float>(kMsPerHour);
        const float tempChange   = hoursElapsed * seg.rampRate;

        if (segmentStartTemp < seg.targetTemperature) {
            setpoint = segmentStartTemp + tempChange;
            if (setpoint >= seg.targetTemperature) {
                setpoint                     = seg.targetTemperature;
                appState.status_.currentState = KilnState::SOAKING;
                segmentStartMs               = now;
            }
        } else if (segmentStartTemp > seg.targetTemperature) {
            setpoint = segmentStartTemp - tempChange;
            if (setpoint <= seg.targetTemperature) {
                setpoint                     = seg.targetTemperature;
                appState.status_.currentState = KilnState::SOAKING;
                segmentStartMs               = now;
            }
        } else {
            setpoint                     = seg.targetTemperature;
            appState.status_.currentState = KilnState::SOAKING;
            segmentStartMs               = now;
        }
    } else if (appState.status_.currentState == KilnState::SOAKING) {
        setpoint                 = seg.targetTemperature;
        const float  cur       = appState.status_.currentTemperature;
        const uint32_t soakMins = (now - segmentStartMs) / kMsPerMinute;
        const bool soakDoneByTemp =
            seg.soakTime > 0 && cur >= seg.targetTemperature;
        if (soakDoneByTemp || soakMins >= seg.soakTime) {
            currentSegmentIdx++;
            if (currentSegmentIdx >= (int)prog->segments.size()) {
                appState.status_.currentState = KilnState::DONE;
                return;
            }
            // currentSegmentIdx is in range: 0 <= idx < segments.size() (just checked).
            FiringSegment& newSeg = prog->segments[currentSegmentIdx];
            segmentStartMs   = now;
            // Schedule anchor: completed segment's target (not measured temp), so a
            // lagging kiln does not stretch the overall profile.
            segmentStartTemp = seg.targetTemperature;
            appState.status_.segmentTimeElapsed = 0;

            if (newSeg.rampRate > 0.f) {
                if (segmentStartTemp < newSeg.targetTemperature) {
                    appState.status_.currentState = KilnState::RAMPING;
                    setpoint                     = segmentStartTemp;
                } else if (segmentStartTemp > newSeg.targetTemperature) {
                    appState.status_.currentState = KilnState::COOLING;
                    setpoint                     = segmentStartTemp;
                } else {
                    appState.status_.currentState = KilnState::SOAKING;
                    setpoint                     = newSeg.targetTemperature;
                }
            } else {
                appState.status_.currentState = KilnState::SOAKING;
                setpoint                     = newSeg.targetTemperature;
            }
            applyTelemetryAndPid(now, setpoint);
            return;
        }
    }

    applyTelemetryAndPid(now, setpoint);
}

void FiringController::updatePID(float setpoint, float currentTemp, uint32_t now) {
    float dt = kInitialPidDt_sec;
    if (pidTimeValid) {
        dt = (now - lastPidMs) / 1000.f;
        if (dt > kMaxPidDt_sec) dt = kMaxPidDt_sec;
        if (dt < kMinPidDt_sec) dt = kMinPidDt_sec;
    }
    lastPidMs     = now;
    pidTimeValid  = true;

    const float error = setpoint - currentTemp;

    if (error > -kIntegralErrorBand_C && error < kIntegralErrorBand_C) {
        errorSum += error * dt;
    }
    if (errorSum > kIntegralCap) errorSum = kIntegralCap;
    if (errorSum < -kIntegralCap) errorSum = -kIntegralCap;

    float dTerm = 0.f;
    if (pidHasPrevious) {
        dTerm = -(currentTemp - lastTemp) / dt;
    } else {
        pidHasPrevious = true;
    }
    dFiltered = kDerivativeLpfAlpha * dTerm + (1.f - kDerivativeLpfAlpha) * dFiltered;

    float output = (kp * error) + (ki * errorSum) + (kd * dFiltered);
    if (output < kPidOutMin) output = kPidOutMin;
    if (output > kPidOutMax) output = kPidOutMax;

    powerOutput->setPower(output / 100.f);

    lastTemp               = currentTemp;
    appState.status_.power = output / 100.f;
}
