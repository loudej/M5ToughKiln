#include "kmeter_iso_bare_wire.h"

namespace {
constexpr uint8_t REG_TEMPERATURE_C       = 0x00;
constexpr uint8_t REG_INTERNAL_TEMP_C     = 0x10;
constexpr uint8_t REG_STATUS              = 0x20;
constexpr uint8_t REG_FIRMWARE_VERSION    = 0xFE;
}  // namespace

bool KMeterIsoBareWire::begin(int8_t sda, int8_t scl, uint32_t freq) {
    _wire.begin(sda, scl, freq);

    // Probe the address a few times — the unit can take a beat to come up
    // after Port A is energized.
    for (int i = 0; i < 10; ++i) {
        _wire.beginTransmission(_addr);
        if (_wire.endTransmission() == 0) {
            _initialized = true;
            return true;
        }
        delay(10);
    }
    _initialized = false;
    return false;
}

bool KMeterIsoBareWire::readRegister(uint8_t reg, uint8_t* out, size_t n) {
    _wire.beginTransmission(_addr);
    _wire.write(reg);
    // Return code intentionally ignored: false-positive `err=2` under clock
    // stretching. Truth is whether the subsequent read returns N bytes.
    (void)_wire.endTransmission(true);

    size_t got = _wire.requestFrom(_addr, (uint8_t)n);
    if (got != n) return false;

    for (size_t i = 0; i < n && _wire.available(); ++i) {
        out[i] = _wire.read();
    }
    return true;
}

bool KMeterIsoBareWire::readInt32LE(uint8_t reg, int32_t& value) {
    uint8_t b[4] = {};
    if (!readRegister(reg, b, sizeof(b))) return false;
    value = (int32_t)((uint32_t)b[0] |
                      ((uint32_t)b[1] << 8) |
                      ((uint32_t)b[2] << 16) |
                      ((uint32_t)b[3] << 24));
    return true;
}

bool KMeterIsoBareWire::readFirmwareVersion(uint8_t& version) {
    return readRegister(REG_FIRMWARE_VERSION, &version, 1);
}

bool KMeterIsoBareWire::readStatus(uint8_t& status) {
    return readRegister(REG_STATUS, &status, 1);
}

bool KMeterIsoBareWire::readCelsius(float& celsius) {
    int32_t raw;
    if (!readInt32LE(REG_TEMPERATURE_C, raw)) return false;
    celsius = raw * 0.01f;
    return true;
}

bool KMeterIsoBareWire::readInternalCelsius(float& celsius) {
    int32_t raw;
    if (!readInt32LE(REG_INTERNAL_TEMP_C, raw)) return false;
    celsius = raw * 0.01f;
    return true;
}
