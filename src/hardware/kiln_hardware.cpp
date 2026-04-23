#include "kiln_hardware.h"
#include <Arduino.h>

void MockKilnHardware::init() {
    lastUpdateMs = millis();
}

float MockKilnHardware::readTemperature() {
    uint32_t now = millis();
    float dt = (now - lastUpdateMs) / 1000.0f;
    lastUpdateMs = now;

    if (relayState) {
        currentTemp += (heatRatePerSec * dt);
    } else {
        // Natural cooling towards ambient (25C)
        if (currentTemp > 25.0f) {
            currentTemp -= (coolRatePerSec * dt);
            if (currentTemp < 25.0f) currentTemp = 25.0f;
        }
    }

    // Add a tiny bit of noise to simulate real sensor readings
    float noise = ((random(100) / 100.0f) - 0.5f) * 0.2f;
    return currentTemp + noise;
}

void MockKilnHardware::setRelay(bool on) {
    relayState = on;
}

bool MockKilnHardware::isRelayOn() const {
    return relayState;
}
