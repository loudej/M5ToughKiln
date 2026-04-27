#ifndef KILN_HARDWARE_H
#define KILN_HARDWARE_H

#include <cstdint>
#include <M5Unified.h>
#include "kiln_sensor_read.h"
#include "kmeter_iso_bare_wire.h"

class IKilnHardware {
public:
    virtual ~IKilnHardware() = default;
    virtual bool init() = 0;
    /// Performs one sensor poll and returns the snapshot (reference is stable until the next call).
    virtual const KilnSensorRead& readSensor() = 0;
    virtual void setRelay(bool on) = 0;
    virtual bool isRelayOn() const = 0;
};

// Mock implementation simulating kiln thermal dynamics
class MockKilnHardware : public IKilnHardware {
private:
    float       currentTemp    = 25.0f;
    bool        relayState     = false;
    uint32_t    lastUpdateMs   = 0;
    KilnSensorRead lastRead{};

    const float heatRatePerSec = 0.25f; // ~900°C/h
    const float coolRatePerSec = 0.05f; // ~180°C/h

public:
    bool init() override;
    const KilnSensorRead& readSensor() override;
    void setRelay(bool on) override;
    bool isRelayOn() const override;
};

// Real hardware: M5Stack KMeter ISO thermocouple unit on Port A.
//
// Driven by KMeterIsoBareWire — a hand-rolled bare-Wire wrapper that ignores
// the ESP32 Arduino I2C driver's spurious `endTransmission` NACK reports under
// clock stretching (which made off-the-shelf libraries unusable).
class KMeterISOHardware : public IKilnHardware {
private:
    KMeterIsoBareWire kmeter;
    KilnSensorRead lastRead{};
    float stickyThermocoupleC = 25.f;
    bool  relayState          = false;
    bool  initialized         = false;
    uint8_t cachedFw          = 0;
    uint8_t cachedI2cAddr     = 0;
    int8_t relayPin           = -1;

public:
    bool init() override;
    const KilnSensorRead& readSensor() override;
    void setRelay(bool on) override;
    bool isRelayOn() const override;
};

#endif // KILN_HARDWARE_H
