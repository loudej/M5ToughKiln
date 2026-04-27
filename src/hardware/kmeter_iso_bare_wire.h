#ifndef KMETER_ISO_BARE_WIRE_H
#define KMETER_ISO_BARE_WIRE_H

#include <Arduino.h>
#include <Wire.h>
#include <cstdint>

#include "kiln_sensor_read.h"

// Minimal driver for the M5Stack KMeter ISO Unit (MAX31855KASA-based, 0x66).
//
// `endTransmission()==2` is `I2C_ERROR_ACK` on the write phase (register select). On this
// unit it often coincides with clock stretching, but field experience shows it is also a
// reliable signal of a stuck slave, failed register latch, or marginal bus — i.e. data
// from `requestFrom` may be stale even when the byte count matches. We therefore treat
// persisting err=2 as a failed read for control/UI (return false), run bus recovery
// (9-clock reset + slow Wire), and escalate to Port A power cycle on Core2/Tough when needed.
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

    // Register 0xFF — firmware’s idea of the unit I²C address (normally 0x66).
    bool readUnitI2cAddress(uint8_t& address);

    // Read the 1-byte status register. 0 = data ready and no fault.
    // Non-zero indicates an open thermocouple or a short to GND/VCC.
    bool readStatus(uint8_t& status);

    // Read the thermocouple temperature in °C. Returns false on bus failure.
    bool readCelsius(float& celsius);

    // Read the chip's internal cold-junction temperature in °C.
    bool readInternalCelsius(float& celsius);

    /// One I²C sweep: STATUS, TC °C/°F, internal °C/°F per M5 register map.
    /// Sets validity flags per field; leaves thermocoupleCelsius untouched (hardware layer).
    void pollRegisters(KilnSensorRead& out);

private:
    TwoWire&   _wire;
    uint8_t    _addr;
    bool       _initialized = false;
    int8_t     _pinSda      = -1;
    int8_t     _pinScl      = -1;
    uint32_t   _clockHz     = 100000U;
    /// Last successful Port A power-cycle time (`millis()`); per-instance rate limit for last resort.
    uint32_t   _lastBusPowerCycleMs = 0;

    /// Repeated empty write + `endTransmission` until slave ACKs (same probe as `begin()`).
    bool probeUntilAck(unsigned retries, uint32_t gapMs);

    /// Bit-bang 9 SCL pulses + STOP to release a stuck slave; then `Wire` is re-inited at `kRecoveryClockHz`.
    bool recoverFromWritePhaseNack();

    /// Core2/Tough: cycle AXP192 Port A bus power via `setExtOutput`, settle, probe ACK, `Wire` @ `kRecoveryClockHz`.
    /// Rate-limited; returns false if unsupported board or too soon since last cycle.
    bool lastResortPortBusPowerCycle();

    // Single write-reg + read-n-bytes attempt. On short read, fills up to 8 bytes into `partialOut`.
    bool tryReadRegisterOnce(uint8_t reg, uint8_t* out, size_t n, uint8_t* txErrOut, size_t* gotOut,
                             uint8_t partialOut[8], unsigned* partialCountOut);

    bool readRegister(uint8_t reg, uint8_t* out, size_t n);

    bool readInt32LE(uint8_t reg, int32_t& value);
};

#endif // KMETER_ISO_BARE_WIRE_H
