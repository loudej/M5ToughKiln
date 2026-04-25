#ifndef KILN_HARDWARE_H
#define KILN_HARDWARE_H

#include <cstdint>
#include <M5Unified.h>
#include "kmeter_iso_bare_wire.h"

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
// Driven by KMeterIsoBareWire — a hand-rolled bare-Wire wrapper that ignores
// the ESP32 Arduino I2C driver's spurious `endTransmission` NACK reports under
// clock stretching (which made off-the-shelf libraries unusable).
//
// Relay output is a stub until a relay module is wired.
class KMeterISOHardware : public IKilnHardware {
private:
    KMeterIsoBareWire kmeter;
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
