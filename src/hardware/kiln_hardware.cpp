#include "kiln_hardware.h"
#include <Arduino.h>
#include <M5Unified.h>

// ── KMeterISOHardware ────────────────────────────────────────────────────────

bool KMeterISOHardware::init() {
    auto sda      = M5.getPin(m5::pin_name_t::port_a_sda);
    auto scl      = M5.getPin(m5::pin_name_t::port_a_scl);
    static constexpr int8_t PORT_B_OUT_GPIO = 26;
    auto relayOut = PORT_B_OUT_GPIO;
    M5.Log.printf("KMeter ISO init: Port A SDA=%u  SCL=%u  |  Port B relay=%d\n",
                  sda, scl, relayOut);

    if (!kmeter.begin(sda, scl)) {
        M5.Log.println("KMeter ISO: NOT found — check Port A connection");
        initialized = false;
        return false;
    }

    if (!kmeter.readFirmwareVersion(cachedFw)) {
        M5.Log.println("KMeter ISO: found but FW read failed");
        initialized = false;
        return false;
    }

    if (!kmeter.readUnitI2cAddress(cachedI2cAddr)) {
        M5.Log.println("KMeter ISO: I2C address register read failed");
        initialized = false;
        return false;
    }

    M5.Log.printf("KMeter ISO: ready (FW=0x%02X  I2C=0x%02X)\n", cachedFw, cachedI2cAddr);

    relayPin = relayOut;
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);

    initialized = true;
    return true;
}

const KilnSensorRead& KMeterISOHardware::readSensor() {
    lastRead               = KilnSensorRead{};
    lastRead.hardwareInitialized = initialized;
    lastRead.firmwareVersion     = cachedFw;
    lastRead.i2cAddressReported  = cachedI2cAddr;

    if (!initialized) {
        lastRead.communicationOk     = false;
        lastRead.thermocoupleCelsius = stickyThermocoupleC;
        return lastRead;
    }

    kmeter.pollRegisters(lastRead);
    // Liveness for UI / JSON: false when status read fails (pollRegisters bails early).
    lastRead.communicationOk = lastRead.statusRegisterValid;

    if (lastRead.controlUsable()) {
        stickyThermocoupleC =
            lastRead.thermocoupleRawCentidegrees * 0.01f;
    }

    lastRead.thermocoupleCelsius = stickyThermocoupleC;

    return lastRead;
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

const KilnSensorRead& MockKilnHardware::readSensor() {
    uint32_t now = millis();
    float dt = (now - lastUpdateMs) / 1000.0f;
    lastUpdateMs = now;

    if (relayState) {
        currentTemp += (heatRatePerSec * dt);
    } else {
        if (currentTemp > 25.0f) {
            currentTemp -= (coolRatePerSec * dt);
            if (currentTemp < 25.0f) currentTemp = 25.0f;
        }
    }

    float noise = ((random(100) / 100.0f) - 0.5f) * 0.2f;
    float tc     = currentTemp + noise;

    lastRead                   = KilnSensorRead{};
    lastRead.hardwareInitialized = true;
    lastRead.communicationOk     = true;
    lastRead.statusRegisterValid = true;
    lastRead.statusRegister      = 0;
    lastRead.thermocoupleSampleValid       = true;
    lastRead.thermocoupleRawCentidegrees   = (int32_t)(tc * 100.f);
    lastRead.thermocoupleCelsius           = tc;
    lastRead.internalSampleValid           = true;
    lastRead.internalCelsius               = 25.f;
    lastRead.thermocoupleFahrenheitValid   = true;
    lastRead.thermocoupleFahrenheit        = tc * 9.f / 5.f + 32.f;
    lastRead.internalFahrenheitValid       = true;
    lastRead.internalFahrenheit            = 77.f;
    lastRead.firmwareVersion               = 0;
    lastRead.i2cAddressReported            = KMeterIsoBareWire::DEFAULT_ADDRESS;

    return lastRead;
}

void MockKilnHardware::setRelay(bool on) {
    relayState = on;
}

bool MockKilnHardware::isRelayOn() const {
    return relayState;
}
