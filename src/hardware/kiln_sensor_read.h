#ifndef KILN_SENSOR_READ_H
#define KILN_SENSOR_READ_H

#include <cstdint>
#include <cstdio>

// Single snapshot from the kiln thermocouple front-end after one I2C poll.
//
// M5Stack KMeter ISO documented map (same family as Unit KMeter / MAX31855 front-end):
//   0x00  TC °C       int32 LE × 0.01 °C
//   0x04  TC °F       int32 LE × 0.01 °F
//   0x10  internal °C int32 LE × 0.01 °C (cold junction)
//   0x14  internal °F int32 LE × 0.01 °F
//   0x20  STATUS      uint8 — M5: 0 = ready / no fault summary; non‑zero = fault
//   0xFE  firmware    uint8
//   0xFF  I²C address uint8 (readback)
//
// There is no separate second “status” register in that table: fault information is
// the byte at 0x20 (plus implicit fault bits inside the MAX31855 conversion, which
// the unit’s MCU normally surfaces via 0x20 / invalid data). Thermocouple fault
// detail on the bare MAX31855 is OC / short-to-GND / short-to-VCC in the chip’s
// 32‑bit SPI word; the ISO unit exposes a compact summary at 0x20.

// How 0x20 maps to faults is not fully documented by M5; we treat bits 0–2 as a
// MAX31855-style fault mask (verify on your wiring). Higher / unknown bits are
// appended as a hex nibble after “+”.
namespace KMeterFaultBits {
static constexpr uint8_t kOpenCircuit    = 0x01;  ///< OC
static constexpr uint8_t kShortToGnd     = 0x02;  ///< SG
static constexpr uint8_t kShortToVcc     = 0x04;  ///< SV
static constexpr uint8_t kDecodedMask    = kOpenCircuit | kShortToGnd | kShortToVcc;
}  // namespace KMeterFaultBits

/// Builds a short line like `"OC SG"` or `"SV+80"` (unknown bits in hex). Empty if status==0.
inline void kilnFormatStatusFaultLine(uint8_t status, char* buf, size_t buflen) {
    if (buflen == 0) return;
    buf[0] = '\0';
    if (status == 0) return;

    int n = 0;
    bool any = false;
    if (status & KMeterFaultBits::kOpenCircuit) {
        n += snprintf(buf + n, buflen - (size_t)n, "%sOC", any ? " " : "");
        any = true;
    }
    if (status & KMeterFaultBits::kShortToGnd) {
        n += snprintf(buf + n, buflen - (size_t)n, "%sSG", any ? " " : "");
        any = true;
    }
    if (status & KMeterFaultBits::kShortToVcc) {
        n += snprintf(buf + n, buflen - (size_t)n, "%sSV", any ? " " : "");
        any = true;
    }

    const uint8_t unknown = status & ~KMeterFaultBits::kDecodedMask;
    if (unknown != 0) {
        snprintf(buf + n, buflen - (size_t)n, "%s+%02X", any ? " " : "", unknown);
    } else if (!any) {
        snprintf(buf, buflen, "??:%02X", status);
    }
}

struct KilnSensorRead {
    bool hardwareInitialized = false;

    /// Last poll: I²C delivered the status byte (device responded to that read).
    bool communicationOk = false;

    bool     statusRegisterValid = false;
    uint8_t  statusRegister      = 0;

    bool    thermocoupleSampleValid = false;
    int32_t thermocoupleRawCentidegrees = 0;

    /// Best estimate for UI / control: last good TC °C while sensor is unhealthy.
    float thermocoupleCelsius = 25.f;

    bool  internalSampleValid = false;
    float internalCelsius     = 25.f;

    bool  thermocoupleFahrenheitValid = false;
    float thermocoupleFahrenheit      = 77.f;

    bool  internalFahrenheitValid = false;
    float internalFahrenheit      = 77.f;

    /// From init (0xFE) — not re-read every poll.
    uint8_t firmwareVersion = 0;
    /// From init (0xFF)
    uint8_t i2cAddressReported = 0;

    /// True when this poll produced a TC conversion we should trust for closed-loop heat.
    bool controlUsable() const {
        return hardwareInitialized && communicationOk && statusRegisterValid &&
               statusRegister == 0 && thermocoupleSampleValid;
    }

    /// Convenience: fault summary byte non-zero (when statusRegisterValid).
    bool deviceReportsFault() const {
        return statusRegisterValid && statusRegister != 0;
    }
};

#endif // KILN_SENSOR_READ_H
