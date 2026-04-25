#ifndef KILN_HARDWARE_H
#define KILN_HARDWARE_H

#include <cstdint>
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedMETER.h>

class IKilnHardware {
public:
    virtual ~IKilnHardware() = default;
    virtual void init() = 0;
    virtual float readTemperature() = 0;
    virtual void setRelay(bool on) = 0;
    virtual bool isRelayOn() const = 0;
};

// Mock implementation simulating kiln thermal dynamics
class MockKilnHardware : public IKilnHardware {
private:
    float currentTemp = 25.0f;
    bool relayState = false;
    uint32_t lastUpdateMs = 0;

    const float heatRatePerSec = 0.25f; // ~900°C/h
    const float coolRatePerSec = 0.05f; // ~180°C/h

public:
    void init() override;
    float readTemperature() override;
    void setRelay(bool on) override;
    bool isRelayOn() const override;
};

// Real hardware: M5Stack KMeter ISO thermocouple unit on Port A.
//
// Uses the M5UnitUnified-based M5Unit-METER library, which drives I2C through
// M5HAL instead of Arduino's Wire. This avoids the long-standing ESP32 Arduino
// Wire bug with repeated-start transactions against STM32-based slaves with
// clock stretching (which is what made the older M5Unit-KMeterISO library fail).
//
// Relay output is a stub until a relay module is wired.
class KMeterISOHardware : public IKilnHardware {
private:
    m5::unit::UnitUnified units;
    m5::unit::UnitKmeterISO unit;
    float lastTemp = 25.0f;
    bool relayState = false;
    bool initialized = false;

public:
    void init() override;
    float readTemperature() override;
    void setRelay(bool on) override;
    bool isRelayOn() const override;
};

#endif // KILN_HARDWARE_H
