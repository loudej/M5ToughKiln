#include "firing_controller.h"
#include "../model/app_state.h"
#include <Arduino.h>

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
}  // namespace

FiringController::FiringController(IKilnHardware* hw, PowerOutput* po)
    : hardware(hw), powerOutput(po) {}

void FiringController::update() {
    const uint32_t now = millis();

    if (now - lastUpdateMs < kUpdateIntervalMs) return;
    lastUpdateMs = now;

    appState.status.currentTemperature = hardware->readTemperature();

    if (appState.status.currentState == KilnState::IDLE || appState.status.currentState == KilnState::ERROR) {
        powerOutput->setEnabled(false);
        segmentStartMs    = 0;
        programStartMs    = 0;
        currentSegmentIdx = 0;
        appState.status.totalTimeElapsed   = 0;
        appState.status.segmentTimeElapsed = 0;
        lastPidMs       = 0;
        pidHasPrevious  = false;
        pidTimeValid    = false;
        dFiltered       = 0.f;
        lastTemp        = 0.f;
        return;
    }

    armPowerIfNeeded(now);
    processSegment(now);
    powerOutput->update();
}

void FiringController::armPowerIfNeeded(uint32_t now) {
    if (powerOutput->getEnabled()) return;

    powerOutput->setEnabled(true);
    programStartMs   = now;
    segmentStartMs   = now;
    segmentStartTemp = appState.status.currentTemperature;
    errorSum         = 0.f;
    lastTemp         = appState.status.currentTemperature;
    dFiltered        = 0.f;
    lastPidMs        = 0;
    pidHasPrevious   = false;
    // No dt sample yet; first updatePID uses kInitialPidDt_sec until pidTimeValid is set.
    pidTimeValid     = false;
}

void FiringController::applyTelemetryAndPid(uint32_t now, float setpoint) {
    appState.status.segmentTimeElapsed = (now - segmentStartMs) / kMsPerMinute;
    appState.status.totalTimeElapsed   = (now - programStartMs) / kMsPerSecond;
    appState.status.targetTemperature  = setpoint;
    updatePID(setpoint, appState.status.currentTemperature, now);
}

void FiringController::processSegment(uint32_t now) {
    FiringProgram* prog = appState.activeProgram();
    if (!prog) {
        appState.status.currentState = KilnState::ERROR;
        return;
    }
    if (currentSegmentIdx >= (int)prog->segments.size()) {
        appState.status.currentState = KilnState::IDLE;
        return;
    }

    FiringSegment& seg = prog->segments[currentSegmentIdx];
    float setpoint = segmentStartTemp;

    if (appState.status.currentState == KilnState::RAMPING) {
        const float hoursElapsed = (now - segmentStartMs) / static_cast<float>(kMsPerHour);
        const float tempChange   = hoursElapsed * seg.rampRate;

        if (segmentStartTemp < seg.targetTemperature) {
            setpoint = segmentStartTemp + tempChange;
            if (setpoint >= seg.targetTemperature) {
                setpoint = seg.targetTemperature;
                appState.status.currentState = KilnState::SOAKING;
                segmentStartMs = now;
            }
        } else {
            setpoint = segmentStartTemp - tempChange;
            if (setpoint <= seg.targetTemperature) {
                setpoint = seg.targetTemperature;
                appState.status.currentState = KilnState::SOAKING;
                segmentStartMs = now;
            }
        }
    } else if (appState.status.currentState == KilnState::SOAKING) {
        setpoint = seg.targetTemperature;
        const uint32_t soakMins = (now - segmentStartMs) / kMsPerMinute;
        if (soakMins >= seg.soakTime) {
            currentSegmentIdx++;
            if (currentSegmentIdx >= (int)prog->segments.size()) {
                appState.status.currentState = KilnState::IDLE;
                return;
            }
            // currentSegmentIdx is in range: 0 <= idx < segments.size() (just checked).
            FiringSegment& newSeg = prog->segments[currentSegmentIdx];
            segmentStartMs   = now;
            // Schedule anchor: completed segment's target (not measured temp), so a
            // lagging kiln does not stretch the overall profile.
            segmentStartTemp = seg.targetTemperature;
            appState.status.segmentTimeElapsed = 0;

            if (newSeg.rampRate > 0.f) {
                appState.status.currentState = KilnState::RAMPING;
                setpoint = segmentStartTemp;
            } else {
                appState.status.currentState = KilnState::SOAKING;
                setpoint = newSeg.targetTemperature;
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

    lastTemp = currentTemp;
}
