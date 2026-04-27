#include "kmeter_iso_bare_wire.h"

#include <M5Unified.h>

#include <algorithm>
#include <cstdio>

namespace {

constexpr uint8_t REG_I2C_ADDRESS       = 0xFF;
constexpr uint8_t REG_TEMPERATURE_C     = 0x00;
constexpr uint8_t REG_TEMPERATURE_F     = 0x04;
constexpr uint8_t REG_INTERNAL_TEMP_C   = 0x10;
constexpr uint8_t REG_INTERNAL_TEMP_F   = 0x14;
constexpr uint8_t REG_STATUS            = 0x20;
constexpr uint8_t REG_FIRMWARE_VERSION  = 0xFE;

constexpr uint32_t kRecoveryClockHz = 10000U;
/// Minimum time between Port A bus power cycles (last resort). Short enough to recover quickly
/// when the sensor is down; long enough to avoid hammering the AXP GPIO / Grove rail.
constexpr uint32_t kLastResortPowerCycleMinMs = 5000U;
/// After `setExtOutput(true)`, wait for KMeter MCU + MAX31855 + I²C stack (cold boot > `begin()` probe).
constexpr uint32_t kPostPowerCycleSettleMs = 1000U;

constexpr unsigned kProbeBeginRetries           = 10;
constexpr uint32_t kProbeBeginGapMs             = 10U;
constexpr unsigned kProbeAfterPowerCycleRetries = 25;
constexpr uint32_t kProbeAfterPowerCycleGapMs   = 40U;

/// IEEE 1149 / I²C-bus clear — clock SCL up to 9× so a stuck slave finishes a partial byte.
void i2cBusResetNineClocks(int8_t sda, int8_t scl) {
    if (sda < 0 || scl < 0)
        return;

    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(10);

    pinMode(scl, OUTPUT);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);

    for (int i = 0; i < 9; ++i) {
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
    }

    // STOP: SDA 0→1 while SCL high
    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(sda, HIGH);
    delayMicroseconds(10);

    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
}

/// Space-separated hex of up to `count` bytes; truncates if `hexSz` is too small.
void fmtHexBytes(const uint8_t* buf, size_t count, char* hex, size_t hexSz) {
    size_t hp = 0;
    for (size_t i = 0; i < count && hp + 4 < hexSz; ++i) {
        const int w = snprintf(hex + hp, hexSz - hp, "%s%02X", i ? " " : "", buf[i]);
        if (w > 0)
            hp += static_cast<size_t>(w);
    }
}

/// Per-register advisory log throttle: one slot per documented register + one “other”.
unsigned advisoryThrottleSlot(uint8_t reg) {
    static constexpr uint8_t kRegs[] = {
        REG_TEMPERATURE_C, REG_TEMPERATURE_F, REG_INTERNAL_TEMP_C, REG_INTERNAL_TEMP_F,
        REG_STATUS, REG_FIRMWARE_VERSION, REG_I2C_ADDRESS,
    };
    for (unsigned i = 0; i < sizeof(kRegs); ++i) {
        if (kRegs[i] == reg)
            return i;
    }
    return sizeof(kRegs);
}

constexpr unsigned kAdvisoryThrottleSlots = 8;  // 7 documented registers + misc

}  // namespace

bool KMeterIsoBareWire::probeUntilAck(unsigned retries, uint32_t gapMs) {
    for (unsigned i = 0; i < retries; ++i) {
        _wire.beginTransmission(_addr);
        if (_wire.endTransmission() == 0)
            return true;
        if (gapMs != 0u && i + 1 < retries)
            delay(gapMs);
    }
    return false;
}

bool KMeterIsoBareWire::begin(int8_t sda, int8_t scl, uint32_t freq) {
    _pinSda  = sda;
    _pinScl  = scl;
    _clockHz = freq;
    _wire.begin(sda, scl, freq);

    // Probe the address a few times — the unit can take a beat to come up
    // after Port A is energized.
    _initialized = probeUntilAck(kProbeBeginRetries, kProbeBeginGapMs);
    return _initialized;
}

bool KMeterIsoBareWire::recoverFromWritePhaseNack() {
    if (_pinSda < 0 || _pinScl < 0)
        return false;

    _wire.end();
    delay(20);
    i2cBusResetNineClocks(_pinSda, _pinScl);
    delay(20);
    _wire.begin(_pinSda, _pinScl);
    _wire.setClock(kRecoveryClockHz);

    return true;
}

bool KMeterIsoBareWire::lastResortPortBusPowerCycle() {
    if (_pinSda < 0 || _pinScl < 0)
        return false;

    const uint32_t now = millis();
    if (_lastBusPowerCycleMs != 0 &&
        (now - _lastBusPowerCycleMs) < kLastResortPowerCycleMinMs) {
        return false;
    }

#if defined(ARDUINO_ARCH_ESP32)
    const m5::board_t board = M5.getBoard();
    if (board != m5::board_t::board_M5StackCore2 && board != m5::board_t::board_M5Tough) {
        return false;
    }

    M5.Log.println(
        "KMeter ISO: last resort — Port A bus power cycle (setExtOutput), probe, Wire @ 10 kHz");
    M5.Power.setExtOutput(false);
    delay(500);
    M5.Power.setExtOutput(true);
    delay(kPostPowerCycleSettleMs);

    _wire.end();
    delay(20);
    _wire.begin(_pinSda, _pinScl);
    _wire.setClock(kRecoveryClockHz);

    if (!probeUntilAck(kProbeAfterPowerCycleRetries, kProbeAfterPowerCycleGapMs)) {
        M5.Log.println(
            "KMeter ISO: no I²C ACK yet after power cycle (probe); retrying register read anyway");
    }

    _lastBusPowerCycleMs = millis();
    return true;
#else
    return false;
#endif
}

bool KMeterIsoBareWire::tryReadRegisterOnce(uint8_t reg, uint8_t* out, size_t n, uint8_t* txErr,
                                            size_t* got, uint8_t partialOut[8],
                                            unsigned* partialCountOut) {
    _wire.beginTransmission(_addr);
    _wire.write(reg);
    *txErr = _wire.endTransmission(true);
    *got   = _wire.requestFrom(_addr, static_cast<uint8_t>(n));

    *partialCountOut = 0;

    if (*got != n) {
        const size_t take = std::min(*got, size_t{8});
        for (size_t i = 0; i < take && _wire.available(); ++i)
            partialOut[(*partialCountOut)++] = static_cast<uint8_t>(_wire.read());
        while (_wire.available())
            (void)_wire.read();
        return false;
    }

    for (size_t i = 0; i < n && _wire.available(); ++i)
        out[i] = static_cast<uint8_t>(_wire.read());
    return true;
}

bool KMeterIsoBareWire::readRegister(uint8_t reg, uint8_t* out, size_t n) {
    uint8_t  txErr        = 0;
    size_t   got          = 0;
    uint8_t  partial[8]{};
    unsigned partialCount = 0;

    bool ok = tryReadRegisterOnce(reg, out, n, &txErr, &got, partial, &partialCount);

    // Hard failure: short read — recovery + one retry when write-phase NACK (2).
    if (!ok && txErr == 2 && recoverFromWritePhaseNack()) {
        M5.Log.println("KMeter ISO: endTransmission=2 + short read — bus recovery, retry at 10 kHz");
        ok = tryReadRegisterOnce(reg, out, n, &txErr, &got, partial, &partialCount);
        _wire.setClock(_clockHz);
        if (!ok && lastResortPortBusPowerCycle()) {
            ok = tryReadRegisterOnce(reg, out, n, &txErr, &got, partial, &partialCount);
            _wire.setClock(_clockHz);
        }
        if (!ok) {
            M5.Log.printf(
                "KMeter ISO I2C read failure after recovery: reg=0x%02X endTransmission=%u requestFrom=%u/%u\n",
                reg, txErr, static_cast<unsigned>(got), static_cast<unsigned>(n));
            return false;
        }
    } else if (!ok) {
        char hex[48]{};
        fmtHexBytes(partial, partialCount, hex, sizeof hex);
        M5.Log.printf(
            "KMeter ISO I2C read failure: reg=0x%02X endTransmission=%u requestFrom=%u/%u partial[%u]=[%s]\n",
            reg, txErr, static_cast<unsigned>(got), static_cast<unsigned>(n),
            static_cast<unsigned>(partialCount), partialCount ? hex : "—");
        return false;
    }

    // Full read but endTransmission==2: write-phase NACK — recover every time (no long cooldown).
    if (ok && txErr == 2) {
        if (recoverFromWritePhaseNack()) {
            M5.Log.println(
                "KMeter ISO: endTransmission=2 (write-phase NACK) — bus recovery, retry @ 10 kHz");
            uint8_t tx2 = 0;
            size_t  g2  = 0;
            if (tryReadRegisterOnce(reg, out, n, &tx2, &g2, partial, &partialCount)) {
                txErr = tx2;
                ok    = true;
            } else {
                M5.Log.printf(
                    "KMeter ISO: recovery retry failed reg=0x%02X endTransmission=%u requestFrom=%u/%u\n",
                    reg, tx2, static_cast<unsigned>(g2), static_cast<unsigned>(n));
                ok    = false;
                txErr = tx2;
            }
            _wire.setClock(_clockHz);

            // If the retried read still reports write-phase NACK (2), bytes may be stale even when
            // `requestFrom` length matches — escalate to Port A power cycle (same as hard `!ok` path).
            const bool escalatePower = !ok || (ok && txErr == 2);
            if (escalatePower && lastResortPortBusPowerCycle()) {
                if (tryReadRegisterOnce(reg, out, n, &txErr, &got, partial, &partialCount)) {
                    ok = true;
                } else {
                    M5.Log.printf(
                        "KMeter ISO: last-resort power cycle retry failed reg=0x%02X endTransmission=%u "
                        "requestFrom=%u/%u\n",
                        reg, txErr, static_cast<unsigned>(got), static_cast<unsigned>(n));
                    ok = false;
                }
                _wire.setClock(_clockHz);
            } else if (escalatePower) {
                static uint32_t s_skipPowerLogMs = 0;
                const uint32_t  t               = millis();
                if (s_skipPowerLogMs == 0 || t - s_skipPowerLogMs >= 10000u) {
                    s_skipPowerLogMs = t;
                    M5.Log.println(
                        "KMeter ISO: last-resort power cycle skipped (cooldown <5s, wrong board, or "
                        "invalid pins)");
                }
            }
        }
    }

    if (!ok)
        return false;

    // Persisting write-phase NACK — do not expose bytes as valid temperatures / status.
    if (txErr == 2) {
        static uint32_t s_nack2PersistLogMs = 0;
        const uint32_t  t                  = millis();
        if (s_nack2PersistLogMs == 0 || t - s_nack2PersistLogMs >= 2000u) {
            s_nack2PersistLogMs = t;
            M5.Log.printf(
                "KMeter ISO: write-phase NACK (err=2) reg=0x%02X — rejecting read\n",
                reg);
        }
        return false;
    }

    if (txErr != 0) {
        static uint32_t s_advRegMs[kAdvisoryThrottleSlots]{};
        static uint16_t s_advSuppReg[kAdvisoryThrottleSlots]{};
        const uint32_t  now = millis();
        const unsigned  slot = advisoryThrottleSlot(reg);

        char hex[24]{};
        fmtHexBytes(out, n, hex, sizeof hex);

        const bool due =
            (s_advRegMs[slot] == 0) || (now - s_advRegMs[slot] >= 2000u);
        if (due) {
            M5.Log.printf(
                "KMeter ISO I2C endTransmission=%u reg=0x%02X bytes[%u]=[%s] (other Wire error; "
                "suppressed %u for this reg)\n",
                txErr, reg, static_cast<unsigned>(n), hex, static_cast<unsigned>(s_advSuppReg[slot]));
            s_advSuppReg[slot] = 0;
            s_advRegMs[slot]   = now;
        } else {
            s_advSuppReg[slot]++;
        }
    }

    return true;
}

bool KMeterIsoBareWire::readInt32LE(uint8_t reg, int32_t& value) {
    uint8_t b[4] = {};
    if (!readRegister(reg, b, sizeof(b)))
        return false;
    value = (int32_t)((uint32_t)b[0] |
                      ((uint32_t)b[1] << 8) |
                      ((uint32_t)b[2] << 16) |
                      ((uint32_t)b[3] << 24));
    return true;
}

bool KMeterIsoBareWire::readFirmwareVersion(uint8_t& version) {
    return readRegister(REG_FIRMWARE_VERSION, &version, 1);
}

bool KMeterIsoBareWire::readUnitI2cAddress(uint8_t& address) {
    return readRegister(REG_I2C_ADDRESS, &address, 1);
}

bool KMeterIsoBareWire::readStatus(uint8_t& status) {
    return readRegister(REG_STATUS, &status, 1);
}

bool KMeterIsoBareWire::readCelsius(float& celsius) {
    int32_t raw;
    if (!readInt32LE(REG_TEMPERATURE_C, raw))
        return false;
    celsius = raw * 0.01f;
    return true;
}

bool KMeterIsoBareWire::readInternalCelsius(float& celsius) {
    int32_t raw;
    if (!readInt32LE(REG_INTERNAL_TEMP_C, raw))
        return false;
    celsius = raw * 0.01f;
    return true;
}

void KMeterIsoBareWire::pollRegisters(KilnSensorRead& out) {
    out.statusRegisterValid = readStatus(out.statusRegister);
    if (!out.statusRegisterValid) {
        out.thermocoupleSampleValid       = false;
        out.internalSampleValid           = false;
        out.thermocoupleFahrenheitValid   = false;
        out.internalFahrenheitValid       = false;
        return;
    }

    int32_t raw = 0;

    out.thermocoupleSampleValid = readInt32LE(REG_TEMPERATURE_C, raw);
    if (out.thermocoupleSampleValid) {
        out.thermocoupleRawCentidegrees = raw;
    }

    out.internalSampleValid = readInt32LE(REG_INTERNAL_TEMP_C, raw);
    if (out.internalSampleValid) {
        out.internalCelsius = raw * 0.01f;
    }

    out.thermocoupleFahrenheitValid = readInt32LE(REG_TEMPERATURE_F, raw);
    if (out.thermocoupleFahrenheitValid) {
        out.thermocoupleFahrenheit = raw * 0.01f;
    }

    out.internalFahrenheitValid = readInt32LE(REG_INTERNAL_TEMP_F, raw);
    if (out.internalFahrenheitValid) {
        out.internalFahrenheit = raw * 0.01f;
    }
}
