#ifndef KILN_HARDWARE_H
#define KILN_HARDWARE_H

#include <cstdint>

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
    float currentTemp = 25.0f; // Ambient start
    bool relayState = false;
    uint32_t lastUpdateMs = 0;

    // Simulated degrees per second heating and cooling
    const float heatRatePerSec = 0.25f; // ~900C / hour
    const float coolRatePerSec = 0.05f; // ~180C / hour

public:
    void init() override;
    float readTemperature() override;
    void setRelay(bool on) override;
    bool isRelayOn() const override;
};

#endif // KILN_HARDWARE_H
