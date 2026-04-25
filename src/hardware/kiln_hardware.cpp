#include "kiln_hardware.h"
#include <Arduino.h>
#include <M5Unified.h>

// ── KMeterISOHardware ────────────────────────────────────────────────────────

bool KMeterISOHardware::init() {
    auto sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto scl = M5.getPin(m5::pin_name_t::port_a_scl);
    M5.Log.printf("KMeter ISO: Port A SDA=%u SCL=%u\n", sda, scl);

    if (!kmeter.begin(sda, scl)) {
        M5.Log.println("KMeter ISO: NOT found — check Port A connection");
        initialized = false;
        return false;
    }

    uint8_t fw = 0;
    if (!kmeter.readFirmwareVersion(fw)) {
        M5.Log.println("KMeter ISO: found but FW read failed");
        initialized = false;
        return false;
    }
    M5.Log.printf("KMeter ISO: ready (FW=0x%02X)\n", fw);
    initialized = true;
    return true;
}

float KMeterISOHardware::readTemperature() {
    if (!initialized) return lastTemp;

    uint8_t status = 0;
    if (kmeter.readStatus(status) && status == 0) {
        float t;
        if (kmeter.readCelsius(t)) {
            lastTemp = t;
        }
    }
    return lastTemp;
}

void KMeterISOHardware::setRelay(bool on) {
    relayState = on;
    // TODO: drive a relay output when hardware is wired
}

bool KMeterISOHardware::isRelayOn() const {
    return relayState;
}

// ── MockKilnHardware ─────────────────────────────────────────────────────────

bool MockKilnHardware::init() {
    lastUpdateMs = millis();
    return true;
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
