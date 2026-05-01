# Proposed Kiln Error Catalog (Design Only)

This document defines proposed error names and operator-facing descriptions for the kiln controller in this project.

- Scope: **documentation/design only**
- Status: **not implemented yet**
- Goal: agree on wording, triggers, and operator actions before code changes

## Design Principles

- Prefer hardware/process faults over generic "software error" messages.
- Use short, explicit operator text suitable for the current UI status line.
- Include immediate safe action for each fault.
- Keep one fault active at a time (latched/frozen) until operator reset.

## Error Format

- Internal code format: `K-xxx` (new scheme for this project)
- Operator-facing line: `ERROR K-xxx: <Error Title>` — **Error Title** is exactly the catalog title on the `## K-xxx …` heading (the words after the code).
- Documentation fields:
  - **Trigger (design intent)**
  - **Meaning / likely causes**
  - **Operator action**

## Proposed Errors

## K-101 Sensor Offline
- **Trigger (design intent):** Thermocouple interface is not usable for a sustained period during an active run.
- **Numeric basis (proposed):**
  - Unusable sample streak: `>= 25` consecutive control ticks
  - Control tick: `100 ms`
  - Fault latch delay: `2.5 s` total
- **Meaning / likely causes:** I2C communication failure, sensor unplugged, unit power issue.
- **Operator action:** Stop heating, inspect Port A wiring and sensor power, then reset and retry.

## K-102 Thermocouple Fault
- **Trigger (design intent):** Sensor reports fault bits indicating open-circuit or short condition.
- **Meaning / likely causes:** Broken thermocouple, reversed polarity, short-to-ground, short-to-VCC.
- **Operator action:** Inspect thermocouple and wiring, repair/replace, then reset.

## K-103 Program Integrity Fault
- **Trigger (design intent):** Active or selected firing program fails structural/range validation before or during run start.
- **Meaning / likely causes:** Missing/empty segments, invalid segment order, out-of-range target/ramp/soak values, incompatible serialized program payload.
- **Operator action:** Rebuild or reload the firing program, verify segment values, then restart.

## K-104 Heat Demand, No Response
- **Trigger (design intent):** In `RAMPING` with an upward slope, controller output remains high but measured temperature does not rise by the minimum required amount within a time window.
- **Numeric basis (proposed):**
  - Heat demand threshold: output `>= 80%`
  - Evaluation window: `10 min`
  - Minimum rise required: `10 F (5.6 C)` over that window
  - Latch condition: two consecutive failed windows (`20 min` total)
- **Meaning / likely causes:** Failed element, relay/SSR not closing, open heater circuit, severe undervoltage.
- **Operator action:** Abort run, verify relay actuation, check element continuity and supply voltage.

## K-105 Ramp Overshoot
- **Trigger (design intent):** During `RAMPING` with upward slope, measured temperature exceeds setpoint by overshoot tolerance for longer than debounce window.
- **Numeric basis (proposed):**
  - Overshoot tolerance: `+27 F (+15 C)` above active setpoint
  - Debounce window: `5 min` (to avoid nuisance trips from PID ringing / thermal oscillation)
- **Meaning / likely causes:** Excessive PID aggressiveness, sensor placement error, thermal lag mismatch.
- **Operator action:** Disable heating, review ramp tuning/placement, then retry.

## K-106 Ramp Over-Temp Hard Stop
- **Trigger (design intent):** During `RAMPING` with upward slope, measured temperature exceeds immediate hard-stop overshoot threshold.
- **Numeric basis (proposed):**
  - Immediate hard-stop threshold: `+54 F (+30 C)` above active setpoint (no debounce)
- **Meaning / likely causes:** Relay stuck ON, severe control instability, major sensor/control mismatch.
- **Operator action:** Keep heating disabled, verify output hardware and sensor integrity before reset.

## K-107 Cannot Hold Setpoint
- **Trigger (design intent):** In `SOAKING` state, temperature remains below setpoint by more than tolerance for longer than hold-recovery allowance, despite sustained heat demand.
- **Numeric basis (proposed):**
  - Soak deficit tolerance: `-18 F (-10 C)` or worse (actual below setpoint)
  - Heat demand threshold: output `>= 70%`
  - Recovery allowance: `5 min`
  - Latch condition: deficit remains past allowance continuously
- **Meaning / likely causes:** Underpowered kiln, failing element, relay not delivering full duty, heat loss beyond design.
- **Operator action:** Abort run, inspect elements/relay and verify kiln can maintain the programmed hold temperature.

## K-108 Soak Overshoot
- **Trigger (design intent):** During `SOAKING`, measured temperature stays above soak setpoint by more than soak overshoot tolerance for longer than soak debounce window.
- **Numeric basis (proposed):**
  - Soak overshoot tolerance: `+18 F (+10 C)` above soak setpoint
  - Debounce window: `2 min` (tighter than ramp because soak should be less volatile)
  - Immediate hard-stop threshold: `+45 F (+25 C)` (no debounce)
- **Meaning / likely causes:** Relay stuck ON, integral windup, thermocouple placement near a local hot spot.
- **Operator action:** Disable heating, verify output control hardware, retune hold behavior before retry.

## K-109 Cooling Below Setpoint
- **Trigger (design intent):** In `COOLING` state, measured temperature remains below the cooling setpoint by more than tolerance for longer than recovery allowance.
- **Numeric basis (proposed):**
  - Cooling deficit tolerance: `-18 F (-10 C)` or worse (actual below setpoint)
  - Recovery allowance: `5 min`
  - Latch condition: deficit remains past allowance continuously
- **Meaning / likely causes:** Cooling profile too aggressive, thermocouple placement in a colder spot, door/vent over-cooling, load thermal split.
- **Operator action:** Abort run, reduce cooling aggressiveness, verify thermocouple placement and cooling procedure.

## K-110 Heating Detected During Cooling
- **Trigger (design intent):** Temperature rises unexpectedly during a cooling/down-ramp segment beyond allowed tolerance.
- **Numeric basis (proposed):**
  - State guard: only while segment is COOLING/down-ramp
  - Rise threshold: `+9 F (+5 C)` net rise
  - Evaluation window: `3 min`
  - Latch condition: threshold met with commanded output `<= 5%`
- **Meaning / likely causes:** Output stuck ON, relay welded, external heat input.
- **Operator action:** Inspect output hardware immediately and confirm relay truly de-energizes.

## K-111 Cooling Too Slow
- **Trigger (design intent):** In COOLING/down-ramp state, measured cooling rate is slower than minimum expected profile tracking rate for an extended window.
- **Numeric basis (proposed):**
  - Minimum expected cooldown rate: `27 F/hr (15 C/hr)` downward
  - Evaluation window: `20 min`
  - Latch condition: measured cooldown stays slower than threshold for full window
  - Suppression band near ambient: disabled below `212 F (100 C)` to avoid nuisance trips near room-temp approach
- **Meaning / likely causes:** Kiln insulation/load prevents schedule, ambient conditions too hot, venting/lid strategy not matching profile.
- **Operator action:** Abort or reprogram with a realistic cooling profile; verify venting/cooling procedure.

## K-113 Invalid Sensor Data
- **Trigger (design intent):** Sensor values are available but out-of-range/physically implausible for a sustained period.
- **Numeric basis (proposed):**
  - Plausible process range: `-58 F to 2372 F (-50 C to 1300 C)`
  - Jump-rate plausibility limit: `> 180 F/min (> 100 C/min)` between accepted samples
  - Latch condition: invalidity persists for `>= 5 s`
- **Meaning / likely causes:** Corrupt conversion, bad calibration, unstable interface.
- **Operator action:** Verify sensor readings at room temperature and with known heat source.

## K-114 Config Integrity Fault
- **Trigger (design intent):** Required persisted controller settings fail validation at load or runtime.
- **Meaning / likely causes:** Corrupt NVS/settings payload, incompatible settings format, invalid global controller parameters.
- **Operator action:** Restore default settings and reconfigure controller options; if needed, re-import valid program data separately.

## Deferred (Not Recommended for First Pass)

These may be useful later but are lower priority for initial implementation:

- Door/lid interlock open (requires dedicated input hardware)
- Mains brownout/undervoltage (requires voltage sensing telemetry)
- Fan/aux output faults (if auxiliary channels are added)

## Suggested Implementation Priority

1. `K-101`, `K-102`, `K-103` (already closest to current architecture)
2. `K-104` through `K-111` (thermal process protection: heating, soaking, cooling)
3. `K-113`, `K-114` (controller diagnostics and integrity)
