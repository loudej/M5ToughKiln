#ifndef KMETER_ISO_BARE_WIRE_H
#define KMETER_ISO_BARE_WIRE_H

#include <Arduino.h>
#include <Wire.h>
#include <cstdint>

// Minimal driver for the M5Stack KMeter ISO Unit (MAX31855KASA-based, 0x66).
//
// Why hand-rolled instead of M5Unit-METER:
//   The ESP32 Arduino Wire driver mis-reports `endTransmission()` as `err=2`
//   (NACK on address) whenever the slave clock-stretches — even when the bytes
//   were transferred successfully. The KMeter ISO's STM32 firmware stretches
//   on every register write, so off-the-shelf drivers that gate on the
//   endTransmission return value reject every transaction. We treat that
//   return as advisory; success is judged by `requestFrom` returning the
//   expected byte count.
//
// Register map (from M5Stack's official I2C protocol document):
//   0x00 4B  TEMPERATURE_CELSIUS         int32 LE  * 0.01 °C  (thermocouple)
//   0x04 4B  TEMPERATURE_FAHRENHEIT      int32 LE  * 0.01 °F
//   0x10 4B  INTERNAL_TEMP_CELSIUS       int32 LE  * 0.01 °C  (cold-junction)
//   0x14 4B  INTERNAL_TEMP_FAHRENHEIT    int32 LE  * 0.01 °F
//   0x20 1B  STATUS                      0 = data ready, non-zero = fault bits
//   0xFE 1B  FIRMWARE_VERSION
//   0xFF 1B  I2C_ADDRESS
//
// The MAX31855 has a native ADC quantization of 0.25 °C, so the lowest two
// hundredths digits will only ever take the values .00 / .25 / .50 / .75.

class KMeterIsoBareWire {
public:
    static constexpr uint8_t DEFAULT_ADDRESS = 0x66;

    explicit KMeterIsoBareWire(TwoWire& wire = Wire,
                               uint8_t address = DEFAULT_ADDRESS)
        : _wire(wire), _addr(address) {}

    // Bring up I2C on the given pins and confirm the device responds. Returns
    // false if no slave ACKs at our address after a short retry window.
    bool begin(int8_t sda, int8_t scl, uint32_t freq = 100000U);

    // True once begin() has completed successfully.
    bool isReady() const { return _initialized; }

    // Read the 1-byte firmware version (cheap liveness check).
    bool readFirmwareVersion(uint8_t& version);

    // Read the 1-byte status register. 0 = data ready and no fault.
    // Non-zero indicates an open thermocouple or a short to GND/VCC.
    bool readStatus(uint8_t& status);

    // Read the thermocouple temperature in °C. Returns false on bus failure.
    bool readCelsius(float& celsius);

    // Read the chip's internal cold-junction temperature in °C.
    bool readInternalCelsius(float& celsius);

private:
    TwoWire& _wire;
    uint8_t  _addr;
    bool     _initialized = false;

    // Common path: write `reg`, then read `n` bytes. Returns true iff the
    // requested number of bytes was actually delivered. The `endTransmission`
    // return code is intentionally ignored (see header comment).
    bool readRegister(uint8_t reg, uint8_t* out, size_t n);

    bool readInt32LE(uint8_t reg, int32_t& value);
};

#endif // KMETER_ISO_BARE_WIRE_H
