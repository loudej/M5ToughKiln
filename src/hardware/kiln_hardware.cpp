#include "kiln_hardware.h"
#include <Arduino.h>
#include <M5Unified.h>

// ── KMeterISOHardware ────────────────────────────────────────────────────────

bool KMeterISOHardware::init() {
    auto sda      = M5.getPin(m5::pin_name_t::port_a_sda);
    auto scl      = M5.getPin(m5::pin_name_t::port_a_scl);
    // M5Unified's _pin_table_port_bc has no entry for board_M5Tough (only
    // board_M5StackCore2), so getPin(port_b_out) returns 0xFFFFFFFF.
    // M5Tough shares Core2 hardware: Port B pin 2 is GPIO 26.
    static constexpr int8_t PORT_B_OUT_GPIO = 26;
    auto relayOut = PORT_B_OUT_GPIO;
    M5.Log.printf("KMeter ISO init: Port A SDA=%u  SCL=%u  |  Port B relay=%d\n",
                  sda, scl, relayOut);

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

    relayPin = relayOut;
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);

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
    if (on == relayState) return;
    relayState = on;
    if (relayPin >= 0) {
        digitalWrite(relayPin, on ? HIGH : LOW);
    }
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
