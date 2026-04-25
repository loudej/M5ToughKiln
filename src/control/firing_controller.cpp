#include "firing_controller.h"
#include <Arduino.h>

FiringController::FiringController(IKilnHardware* hw) : hardware(hw) {}

void FiringController::update() {
    uint32_t now = millis();
    
    // Always read current temperature
    appState.status.currentTemperature = hardware->readTemperature();

    // Check if we just started
    if (appState.status.currentState == KilnState::RAMPING && currentSegmentIdx == 0 && segmentStartMs == 0) {
        programStartMs = now;
        segmentStartMs = now;
        segmentStartTemp = appState.status.currentTemperature;
        windowStartMs = now;
        errorSum = 0;
    }

    if (appState.status.currentState == KilnState::IDLE || appState.status.currentState == KilnState::ERROR) {
        hardware->setRelay(false);
        segmentStartMs = 0;
        programStartMs = 0;
        currentSegmentIdx = 0;
        appState.status.totalTimeElapsed   = 0;
        appState.status.segmentTimeElapsed = 0;
        return;
    }

    // Process active program
    processSegment();
    
    // Time proportioning logic
    uint32_t elapsedInWindow = now - windowStartMs;
    if (elapsedInWindow >= windowSizeMs) {
        windowStartMs = now;
        elapsedInWindow = 0;
    }

    // Minimum ON/OFF enforcement (3 seconds) to protect mechanical contactor
    bool turnOn = (elapsedInWindow < relayOnTimeMs);
    
    if (relayOnTimeMs > 0 && relayOnTimeMs < 3000) relayOnTimeMs = 3000;
    if (relayOnTimeMs > (windowSizeMs - 3000) && relayOnTimeMs < windowSizeMs) relayOnTimeMs = windowSizeMs - 3000;

    hardware->setRelay(turnOn);
}

void FiringController::processSegment() {
    if (appState.activeProgramIndex < 0) {
        appState.status.currentState = KilnState::ERROR;
        return;
    }

    // Determine if using predefined or custom
    FiringProgram* prog;
    if (appState.activeProgramIndex < appState.predefinedPrograms.size()) {
        prog = &appState.predefinedPrograms[appState.activeProgramIndex];
    } else {
        int customIdx = appState.activeProgramIndex - appState.predefinedPrograms.size();
        if (customIdx < appState.customPrograms.size()) {
            prog = &appState.customPrograms[customIdx];
        } else {
            appState.status.currentState = KilnState::ERROR;
            return;
        }
    }

    if (currentSegmentIdx >= prog->segments.size()) {
        // Program finished
        appState.status.currentState = KilnState::IDLE;
        return;
    }

    FiringSegment& seg = prog->segments[currentSegmentIdx];
    uint32_t now = millis();
    appState.status.segmentTimeElapsed = (now - segmentStartMs) / 60000; // minutes, compared against soakTime
    appState.status.totalTimeElapsed   = (now - programStartMs) / 1000;  // seconds, used by UI for elapsed/remaining/bar

    // Calculate setpoint
    float setpoint = segmentStartTemp;
    if (appState.status.currentState == KilnState::RAMPING) {
        // Calculate expected temp based on ramp rate (C/h)
        float hoursElapsed = (now - segmentStartMs) / 3600000.0f;
        float tempChange = hoursElapsed * seg.rampRate;
        
        if (segmentStartTemp < seg.targetTemperature) {
            setpoint = segmentStartTemp + tempChange;
            if (setpoint >= seg.targetTemperature) {
                setpoint = seg.targetTemperature;
                appState.status.currentState = KilnState::SOAKING;
                segmentStartMs = now; // Reset timer for soak
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
            // Move to next segment
            currentSegmentIdx++;
            appState.status.currentState = KilnState::RAMPING;
            segmentStartMs = now;
            segmentStartTemp = appState.status.currentTemperature;
            return;
        }
    }

    appState.status.targetTemperature = setpoint;
    updatePID(setpoint, appState.status.currentTemperature);
}

void FiringController::updatePID(float setpoint, float currentTemp) {
    float error = setpoint - currentTemp;
    
    // Prevent integral windup
    if (error > -50 && error < 50) {
        errorSum += error;
    }
    
    float dError = error - lastError;
    lastError = error;

    float output = (kp * error) + (ki * errorSum) + (kd * dError);

    // Constrain output between 0% and 100% effort
    if (output < 0) output = 0;
    if (output > 100) output = 100;

    // Convert effort to relay ON time within the 60s window
    relayOnTimeMs = (uint32_t)((output / 100.0f) * windowSizeMs);
}
