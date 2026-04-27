# Kiln Controller

ESP32-based kiln firing controller running on an M5Stack Tough, with a
thermocouple temperature sensor and a 240 V AC contactor output.

---

## Hardware

### Controller — M5Stack Tough

| Item | Detail |
|---|---|
| MCU | ESP32-D0WDQ6-V3 @ 240 MHz |
| Display | 2.0" 320×240 ILI9342C touch LCD |
| Touch | FT6336U capacitive |
| Firmware UI | LVGL 9.x |

### Thermocouple Interface — M5Stack KMeter ISO Unit

Connected to **Port A** (I²C).

| Item | Detail |
|---|---|
| Chip | MAX31855KASA |
| Thermocouple type | K-type |
| Temperature range | −200 °C to +1350 °C |
| Native resolution | 0.25 °C (14-bit ADC) |
| Sample rate | ≤ 10 Hz |
| I²C address | 0x66 |
| Port A pins | SDA = GPIO 32, SCL = GPIO 33 |
| Driver | Custom bare-Wire wrapper (`kmeter_iso_bare_wire`) |

> **I²C note:** The KMeter ISO's STM32 firmware stretches the clock on every
> register write. The ESP32 Arduino `Wire` driver mis-reports this as a NACK
> (`endTransmission` returns 2) even though bytes transfer correctly. The
> bare-Wire driver ignores that return value and judges success by whether
> `requestFrom` delivers the expected byte count.

### Power Output — Baomain HC1-50 (BCT-50) Contactor

Connected to **Port B** output pin (GPIO 26).

| Item | Detail |
|---|---|
| Model | HC1-50 / BCT-50 |
| Poles | 4-pole, 2NO 2NC wired as 2-pole for kiln element |
| Rated voltage (Ue) | 250 V AC |
| Rated current | 50 A |
| Coil voltage | 220 V AC |
| Mounting | 35 mm DIN rail |
| Dimensions | 82 × 36 × 62 mm |
| Standard | IEC 60947-4-1 household contactor class |

**⚠️ No datasheet available.** Baomain does not publish electrical life,
mechanical life, minimum on/off dwell time, or maximum switching frequency for
the HC1-50. The product page lists only the physical and voltage specs above.
Contact Baomain at info@baomain.com to request a datasheet before making any
firmware decisions about minimum switching times or expected service life.

---

## Pin Map

| Function | Port | GPIO |
|---|---|---|
| KMeter ISO SDA | Port A | 32 |
| KMeter ISO SCL | Port A | 33 |
| Contactor coil output | Port B pin 2 | 26 |
| Port B input (unused) | Port B pin 1 | 36 (input-only) |

---

## Firmware Architecture

```
main.cpp
├── FiringController        — program segments + PID
├── PowerOutput             — 60s time-proportioning relay control
├── KMeterISOHardware       — thermocouple read + relay GPIO
│   └── KMeterIsoBareWire   — bare-Wire I²C driver for KMeter ISO
├── LVGL UI
│   ├── ui_main_screen      — live temp, status, power%, progress
│   ├── ui_program_config_screen
│   ├── ui_settings_screen  — °F / °C unit selection (NVS-persisted)
│   └── ui_edit_segment_popup
└── ProfileGenerator        — Bartlett kiln schedules + custom programs
```

### Time-Proportioning

`PowerOutput` uses a **60-second window**. At X% power the relay is ON for
`X * 0.6` seconds and OFF for `(100 − X) * 0.6` seconds each window. Example:

| Power | ON time | OFF time |
|---|---|---|
| 10% | 6 s | 54 s |
| 50% | 30 s | 30 s |
| 90% | 54 s | 6 s |

### PID Parameters (in `firing_controller.cpp`)

| Parameter | Value | Notes |
|---|---|---|
| Kp | 2.0 | Proportional gain |
| Ki | 0.01 | Integral gain |
| Kd | 10.0 | Derivative gain |
| Window | 60 s | Time-proportioning period |
| Min dwell | 3 s | Minimum relay on/off time |

---

## Building

```
pio run --target upload --target monitor --environment m5stack-core2
```

Dependencies (resolved automatically by PlatformIO):
- `m5stack/M5Unified @ 0.2.5`
- `lvgl/lvgl @ ^9.5.0`
