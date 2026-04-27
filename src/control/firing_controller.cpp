#include "firing_controller.h"
#include "../model/app_state.h"
#include <Arduino.h>

FiringController::FiringController(IKilnHardware* hw, PowerOutput* po)
    : hardware(hw), powerOutput(po) {}

void FiringController::update() {
    uint32_t now = millis();

    appState.status.currentTemperature = hardware->readTemperature();

    if (appState.status.currentState == KilnState::IDLE || appState.status.currentState == KilnState::ERROR) {
        powerOutput->setEnabled(false);
        segmentStartMs   = 0;
        programStartMs   = 0;
        currentSegmentIdx = 0;
        appState.status.totalTimeElapsed   = 0;
        appState.status.segmentTimeElapsed = 0;
        return;
    }

    // Arm output on first frame of a new run
    if (!powerOutput->getEnabled()) {
        powerOutput->setEnabled(true);
        programStartMs    = now;
        segmentStartMs    = now;
        segmentStartTemp  = appState.status.currentTemperature;
        errorSum          = 0;
    }

    processSegment();
    powerOutput->update();
}

void FiringController::processSegment() {
    if (appState.activeProgramIndex < 0) {
        appState.status.currentState = KilnState::ERROR;
        return;
    }

    FiringProgram* prog;
    if (appState.activeProgramIndex < (int)appState.predefinedPrograms.size()) {
        prog = &appState.predefinedPrograms[appState.activeProgramIndex];
    } else {
        int customIdx = appState.activeProgramIndex - (int)appState.predefinedPrograms.size();
        if (customIdx < (int)appState.customPrograms.size()) {
            prog = &appState.customPrograms[customIdx];
        } else {
            appState.status.currentState = KilnState::ERROR;
            return;
        }
    }

    if (currentSegmentIdx >= (int)prog->segments.size()) {
        appState.status.currentState = KilnState::IDLE;
        return;
    }

    FiringSegment& seg = prog->segments[currentSegmentIdx];
    uint32_t now = millis();
    appState.status.segmentTimeElapsed = (now - segmentStartMs) / 60000; // minutes
    appState.status.totalTimeElapsed   = (now - programStartMs) / 1000;  // seconds

    float setpoint = segmentStartTemp;
    if (appState.status.currentState == KilnState::RAMPING) {
        float hoursElapsed = (now - segmentStartMs) / 3600000.0f;
        float tempChange   = hoursElapsed * seg.rampRate;

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
        if (appState.status.segmentTimeElapsed >= seg.soakTime) {
            currentSegmentIdx++;
            appState.status.currentState = KilnState::RAMPING;
            segmentStartMs   = now;
            segmentStartTemp = appState.status.currentTemperature;
            return;
        }
    }

    appState.status.targetTemperature = setpoint;
    updatePID(setpoint, appState.status.currentTemperature);
}

void FiringController::updatePID(float setpoint, float currentTemp) {
    float error = setpoint - currentTemp;

    if (error > -50 && error < 50) {
        errorSum += error;
    }

    float dError = error - lastError;
    lastError = error;

    float output = (kp * error) + (ki * errorSum) + (kd * dError);
    if (output < 0.0f)   output = 0.0f;
    if (output > 100.0f) output = 100.0f;

    powerOutput->setPower(output / 100.0f);
}
