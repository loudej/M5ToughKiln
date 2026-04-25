#include "kiln_hardware.h"
#include <Arduino.h>
#include <Wire.h>
#include <M5Unified.h>
#include <cmath>

// ── KMeterISOHardware ────────────────────────────────────────────────────────
// Driven by M5UnitUnified / M5HAL — bypasses Arduino Wire's broken
// repeated-start handling. M5Unified manages its own internal-bus driver, so
// Arduino's Wire object is free to be repurposed for Port A.

void KMeterISOHardware::init() {
    auto sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto scl = M5.getPin(m5::pin_name_t::port_a_scl);
    M5.Log.printf("KMeter ISO: Port A SDA=%u SCL=%u\n", sda, scl);

    Wire.begin(sda, scl, 100000U);

    // ACK probe loop — gives the unit time to come up after Port A is powered.
    for (int i = 0; i < 10; ++i) {
        Wire.beginTransmission(unit.address());
        if (Wire.endTransmission() == 0) break;
        delay(10);
    }

    if (!units.add(unit, Wire) || !units.begin()) {
        M5.Log.println("KMeter ISO: NOT found — check Port A connection");
        initialized = false;
        return;
    }

    M5.Log.println("KMeter ISO: ready");
    initialized = true;
}

float KMeterISOHardware::readTemperature() {
    if (!initialized) return lastTemp;

    // Drives the periodic measurement state machine inside the library.
    units.update();

    float t = unit.temperature();
    if (!std::isnan(t)) {
        lastTemp = t;
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
