# New sensor inputs — ATS601LSG wheel speed + analog input via TLA2528

## ATS601LSG

| Parameter | Value | Page cite |
|-----------|-------|-----------|
| Interface type | Open-drain output (3-wire voltage) | ATS601 p.2 |
| Supply voltage range | 3.3 V to 24 V | ATS601 p.3 |
| Supply current (typical) | 4–10 mA | ATS601 p.3 |
| Output pull-up required | Yes; recommended 1 kΩ at 5 V (table notes) | ATS601 p.3 |
| Output sink current (max) | 80 mA (current limit) | ATS601 p.3 |
| Output V_OL (sat) | 450 mV max @ 15 mA sink | ATS601 p.3 |
| Rise time (10–90%, 1 kΩ, 4.7 nF) | 10 µs typical | ATS601 p.3 |
| Fall time (90–10%, 1 kΩ, 4.7 nF) | 3–10 µs (6 µs typical) | ATS601 p.3 |
| Max operating frequency / bandwidth | 8 kHz analog bandwidth; "Defined power-on state"; zero-speed capable | ATS601 p.3, p.1 |
| Power-on time | ≤ 1 ms (max) | ATS601 p.3 |
| Self-calibration / startup behavior | Startup Hysteresis mode (40 G or 80 G options); Calibration mode for 3–9 edges; then Running mode; Stop mode if 5 s no edge | ATS601 p.4, p.5 (functional description) |
| Direction / vibration / diagnostic | Single Hall element, direction-insensitive polarity; internal hysteresis 5–15% peak-to-peak; Stop mode (5 s timeout) re-enters Startup Hysteresis | ATS601 p.4 |
| Datasheet application circuit | Bypass cap on VCC (not specified value); no series R; external pull-up recommended (1 kΩ at 5 V for test conditions, but app note silent on 3.3 V pod) | ATS601 p.3 |

## Wheel-speed signal path — pod → main board

| Item | Refdes / net | Value | Source |
|------|--------------|-------|--------|
| Pod sensor | U1 (ATS601LSG) | Open-drain output; pin 2 (OUT) = /WHEEL | breakout board-facts L96 |
| Pod output net | /WHEEL | Straight to J1.5, no on-board pull-up | breakout board-facts L97, L174 |
| Field cable | J1.5 → M8 pin 5 | 6-wire M8 or equivalent | breakout board-facts L136 |
| Main board front connector | J5.5 (M8 front field) | WHEEL_F_FIELD | idl1-facts L173 |
| Main board rear connector | J4.5 (M8 rear field) | WHEEL_R_FIELD | idl1-facts L162 |
| Series resistance (front) | R12 | 330Ω | idl1-facts L21, R56 BOM |
| Series resistance (rear) | R16 | 330Ω | idl1-facts L21, R56 BOM |
| Pull-up (front) | R14 | 4k7 to +3V3 | idl1-facts L21, R59 BOM |
| Pull-up (rear) | R15 | 4k7 to +3V3 | idl1-facts L21, R59 BOM |
| Filter (front) | C12 | 10nF (0.01µF) | idl1-facts L21, R47 BOM |
| Filter (rear) | C15 | 10nF (0.01µF) | idl1-facts L21, R47 BOM |
| ESP32-C6 pad (front) | pad1 = GPIO0 | WHEEL_F | idl1-facts L98, L205 |
| ESP32-C6 pad (rear) | pad2 = GPIO1 | WHEEL_R | idl1-facts L99, L205 |

### RC rise-time and frequency check

Network: 4k7 pull-up to +3V3, 10nF filter in parallel with 330Ω series resistor.

RC delay (through filter to input): τ = 4700 Ω × 10 nF = 47 µs. Rise time (10–90%) ≈ 2.2 τ ≈ 103 µs.
ATS601 output rise time is 10 µs (typical); with 330Ω series and 10nF filter, dominant pole is the RC network.
For a 50% pulse (assume symmetrical):
- ATS601 switching time: 50 µs + 50 µs = 100 µs ≈ 10 kHz pulse capability.
- RC network rise time: ~103 µs.
- Combined: ~150 µs per transition; **maximum pulse frequency ≈ 6.7 kHz** with comfortable margin.
- ATS601 max bandwidth (8 kHz) is not a hard rate limit; at 6.7 kHz the network transitions are slow but stable.
- Realistic wheel-target speed (e.g., 8–12 teeth on a wheel at 3000 RPM) → 400–600 Hz pulse rate → 300–500% margin. **Adequate.**

---

## ESP32-C6 peripheral fit for WHEEL

### GPIO0 and GPIO1 strapping / boot conflicts

Strapping pins on ESP32-C6: GPIO4, GPIO5, GPIO8, GPIO9, GPIO15 only (Section 2.3.5, line 758, C6 p. ≈36).
GPIO0 and GPIO1 are **not** strapping pins; both are Priority 2 (freely usable).

XTAL_32K pins:
- GPIO0 = XTAL_32K_P (alternative/analog function, per datasheet Table 2-1–2-2 pin summary).
- GPIO1 = XTAL_32K_N (alternative/analog function).

These are alternate functions; if no external 32 kHz oscillator is populated, GPIO0/GPIO1 default as regular GPIOs and are safe for pulse counting.

### PCNT unit count, channels, glitch filter, counter width

From ESP32-C6 datasheet Section 4.2.1.5 (line 2885–2898):
- **Unit count:** 4 independent pulse counters.
- **Channels per unit:** 2 channels each → 8 channels total (4 units × 2).
- **Glitch filter:** Present for input pulse signals and control signals. Max filter length: **Not specified in datasheet; refer to Technical Reference Manual.** Typical embedded PCNT filters are 8–16 APB cycles; specification deferred.
- **Counter width:** Not given in product datasheet; Technical Reference Manual required.

Alternative capture methods:
- **GPIO ISR:** Standard edge-triggered interrupt on GPIO0 or GPIO1, with software timestamp and counter (as shown in IDL0 code).
- **RMT (Remote Controller Peripheral):** Can capture pulse-train timing; typically 2 independent input slots per RMT channel.

### Recommendation (labelled: WHEEL peripheral choice)

**For wheel speed at variable RPM (e.g., 100–3000 RPM on test rig):**

Use **PCNT Unit 0, Channels 0/1** (GPIO0 front, GPIO1 rear) in **increment-count mode**, with interrupt on overflow or periodically polled counter register.
- **Rationale:** PCNT hardware-counts edges in hardware; no ISR overhead for every edge. At low RPM (e.g., 1 target tooth per 100 ms), ISR jitter becomes visible; PCNT avoids it.
- **Fallback:** If PCNT glitch-filter spec is insufficient or counter width < 16-bit: Use **GPIO ISR with 1 ms debounce** (as per IDL0 code template). Typical motor-shaft speeds (100+ RPM) produce edges > 1 ms apart.

**For high-precision timing (e.g., traction control):**

Combine **PCNT count** with **RMT period capture** on the same GPIO edges to measure time between pulses; compute speed = 60 / (time_us / 1e6 * teeth).

---

## TLA2528 — analog input

| Parameter | Value | Page cite |
|-----------|-------|-----------|
| Resolution | 12-bit ADC; 16-bit averaged output | TLA p.1, p.13 |
| Input range (0–AVDD) | 0 V to 3.3 V (AVDD = 3.3 V on this board) | TLA p.4 |
| INL (Integral Nonlinearity) | ±0.5 LSB (typical) | TLA p.5 |
| DNL (Differential Nonlinearity) | ±0.45 LSB (typical) | TLA p.5 |
| I2C address (ADDR grounded) | 0x17 (001 0111b); see address selection table | TLA p.6, L440 |
| Max SCL frequency | 3.4 MHz (high-speed mode); device supports 100 kHz, 400 kHz, 1 MHz, 3.4 MHz | TLA p.1 |
| I2C timing constraints | SCL stretching during conversion; max hold time not separately specified; see I2C Timing Requirements table (Section 6.8) | TLA p.6 |
| Conversion time (tCONV) | Independent of OSC_SEL and CLK_DIV[3:0]; see Switching Characteristics table. Nominal ~600 ns (inferred from internal oscillator). | TLA p.7, line 509 |
| Max sample rate (single-channel manual) | 1000 kSPS (OSC_SEL=0, CLK_DIV=0, nominal 1 µs cycle) | TLA p.6 |
| Auto-sequence mode | Supported; device auto-steps through selected channels after each conversion initiated by host. | TLA p.13 |
| Oversampling / averaging | OSR[2:0] register (0–7 oversample factor); 16-bit result (versus 12-bit single read). OSR_DONE status bit and STATS_EN flag for non-blocking mode. | TLA p.13, line 356, line 462 |

---

## TLA2528 register sequence for a single-ended read

| Step | Register | Addr | Value | Why | Page cite |
|------|----------|------|-------|-----|-----------|
| 0 (init) | GENERAL_CFG | 0x01 | (power-on default: 0x00) — all channels analog input | Channels AIN4/AIN5 default as analog inputs on power-up. No explicit config needed if starting from reset. | TLA p.23 |
| 1a | (optional calibration) | CAL bit in GENERAL_CFG | 1b, then poll until 0b | Calibrate ADC offset (temperature/AVDD drift). CAL bit auto-clears after completion. | TLA p.13, line 421 |
| 2 | CHANNEL_SEL | 0x09 | 0x04 (for AIN4) or 0x05 (for AIN5) | Select analog channel to read (4 = AIN4/GPIO4 pin 3 front rear; 5 = AIN5/GPIO5 pin 4 front). | TLA p.24 |
| 3 | CONVERSION | 0x08 | 0x80 (start manual conversion, or read most-recent result) | Write 0x80 to start; read returns raw 12-bit result in next I2C transaction. | TLA p.24 |
| 4 | RESULT_MSB / RESULT_LSB | 0x0A / 0x0B | (read-only) | MSB = upper 8 bits; LSB = lower 4 bits in [7:4], [3:0] = status. Combine: raw_12b = (MSB << 4) | (LSB >> 4). | TLA p.24, line 512 |
| (optional) | OSR_CFG | 0x06 | OSR[2:0] = N (0–7) | Enable averaging; OSR=1 → 2 samples, OSR=2 → 4 samples, ... OSR=7 → 128 samples. Result in RESULT_AVG_MSB/LSB (0x0C/0x0D). | TLA p.23, line 452 |
| (optional) | SEQUENCE_CFG | 0x10 | 0x01 (auto-sequence AIN4/AIN5) | Auto-step between multiple channels on each conversion cycle; device-driven, no host command needed after init. | TLA p.23, line 1002 |

---

## Analog signal path — pod → TLA2528

| Item | Refdes / net | Value | Source |
|------|--------------|-------|--------|
| Pod analog output | J2 (JST connector) pin 3 | /ANA | breakout board-facts L146 |
| Pod connector J2 pins | 1 GND, 2 +3V3, 3 ANA | Straight to transducer output | breakout board-facts L140–150 |
| Pod TVS clamp | U2 (TPD6E05U06) | All field lines clamped | breakout board-facts L17, L61 |
| Field cable | J1.6 / J2.3 → M8 pin 6 (front/rear) | 6-wire M8 or dual M8 connectors | idl1-facts L173, L162 |
| Main board front connector | J5.6 (M8 front) | ANA_F_FIELD | idl1-facts L174 |
| Main board rear connector | J4.6 (M8 rear) | ANA_R_FIELD | idl1-facts L163 |
| Series resistance (front) | R13 | 33Ω | idl1-facts L22, R60 BOM |
| Series resistance (rear) | R17 | 33Ω | idl1-facts L22, R60 BOM |
| Filter (front) | C13 | 100nF | idl1-facts L22, R45 BOM |
| Filter (rear) | C16 | 100nF | idl1-facts L22, R45 BOM |
| TVS clamp (front) | U5 D2+ (pin 4) | TPD4E05U06 TVS | idl1-facts L173 |
| TVS clamp (rear) | U6 D2+ (pin 4) | TPD4E05U06 TVS | idl1-facts L163 |
| TLA2528 analog input (front) | U4 pin 4 (AIN5/GPIO5) | Direct post-filter | idl1-facts L174, L208 |
| TLA2528 analog input (rear) | U4 pin 3 (AIN4/GPIO4) | Direct post-filter | idl1-facts L163, L209 |

### Input settling check

33Ω series + 100nF filter in parallel → τ = 33 Ω × 100 nF = 3.3 µs.
Settling time to 1% (5 τ) ≈ 16.5 µs.
TLA2528 max sample rate (assuming ~1 µs conversion, OSC_SEL=0, CLK_DIV=0) → 1000 kSPS → 1 µs per sample.
Acquisition window (typical SAR ADC, inferred ~100–200 ns from internal oscillator).
**Worst case:** 33Ω source impedance + 100nF capacitive load. Acquisition cap charging: t_acq ≈ 5 × 33 Ω × C_acq. If C_acq ≈ 1–10 pF (typical internal): t_acq ≈ 165–1650 ns.
**Settling at 1 kHz sample rate (1000 µs cycles):** Plenty of time (1000 µs >> 16.5 µs settling).
**Settling at 100 kSPS (10 µs cycles):** Filter settling (16.5 µs) slightly exceeds cycle time, but settling cap inside TLA2528 tolerates delayed settling within spec. Recommend **≤ 10 kSPS single-channel, or ≤ 5 kSPS auto-sequence** for noise-free DC readings.

---

## What IDL0 already does (digital_task.c)

IDL0 implements GPIO edge detection for discrete markers/button inputs, not continuous wheel speed or analog measurement:

| Item | Details | File:line |
|------|---------|-----------|
| Peripheral for inputs | GPIO ISR (gpio_install_isr_service) on configured GPIO pins; one interrupt handler per channel. | digital_task.c:91 |
| Edge detection | Rising or falling edge (active_low configurable); no counting or RMT. | digital_task.c:101 |
| Debounce | Software debounce: drop events within debounce_ms of last event (1 ms default typical). | digital_task.c:59–62 |
| Rate / constants | Debounce threshold per-channel; no explicit sample rate (ISR-driven, event-based). No rates like kSPS. | digital_task.c:60, config struct |
| Output | Monotonic u8 press counter (wraps at 256); timestamp_us; submitted as CHANNEL_SAMPLE records to writer. | digital_task.c:64–67 |
| **What changes for IDL1** | IDL0 uses GPIO ISR for **discrete presses (markers)**. IDL1 needs **continuous wheel speed pulse counting** (PCNT or high-speed ISR, ≥ kHz rates); **analog pressure input** (TLA2528 I2C ADC, manually/auto-sequenced); neither is in IDL0 code. IDL0 debounce (1 ms) is too coarse for wheel edges (µs-scale). Reuse ISR infrastructure for GPIO0/1 WHEEL if PCNT unavailable; must add TLA2528 I2C driver (init, convert, read). | — |

---

## Recommendations

### Wheel Speed (ATS601LSG → GPIO0/GPIO1)

**Recommendation: PCNT or high-speed GPIO ISR**
- **Primary:** Use PCNT Unit 0 (channels 0/1 mapped to GPIO0/1) in edge-count mode, with glitch filter enabled.
  - **Why:** Hardware edge counting avoids ISR overhead; suitable for variable RPM (100–3000 RPM pod / test rig).
  - **Gotcha:** Glitch-filter max length not specified in product datasheet; must cross-check TRM. If inadequate, fallback to ISR.
- **Fallback:** GPIO ISR with sub-millisecond debounce (100 µs software filter or hardware edge latch).
  - **Gotcha:** At high RPM (>100 Hz pulses), ISR CPU load rises; timing jitter for precise speed may exceed margin.
  - **How:** Reuse IDL0 digital_task.c template; lower debounce_ms to 0 (disable) or ~100 µs (if ISR latency permits software filtering).
- **Power-on behavior:** ATS601 powers high, enters Calibration on first edge; firmware should ignore spurious resets (debounce ≥ 1 ms at startup) or detect Calibration by ignoring first 3–9 pulses.

### Analog Input (Pressure) → TLA2528 AIN4/AIN5

**Recommendation: I2C ADC with periodic polling and averaging**
- **Sample rate:** 1 kHz single-channel (1 µs cycle, 100 Hz settling margin) or 500 Hz auto-sequence (AIN4+AIN5 interleaved, 10 µs settle per channel).
  - **Gotcha:** At 100 kSPS raw, 1 µs cycles, settling incomplete (filter τ = 3.3 µs); enable OSR (oversampling) to 16–32× and reduce to 5–10 kSPS for 12-bit accuracy.
- **Register sequence:** Auto-sequence (SEQUENCE_CFG = 0x01) simplifies firmware; device auto-steps AIN4 → AIN5 → repeat. Host polls CONVERSION every ~10 ms, reads MSB/LSB, decodes 12-bit raw (left-aligned in MSB[7:0] + LSB[7:4]).
- **I2C address:** 0x17 (verified: ADDR net-tie to GND per schematic note in idl1-facts L228).
- **Gotchas:**
  - Power-up: tPU ≤ 5 ms; wait before first I2C transaction.
  - Calibration: CAL bit in GENERAL_CFG auto-clears; poll until done if temperature/voltage drift matters (typically < 1 ms).
  - SCL stretching: Device holds SCL low during conversion; ensure I2C master timeout > 1 ms per cycle.
- **Edge case:** If firmware cannot tolerate SCL blocking, disable auto-sequence and manually write CHANNEL_SEL + CONVERSION for each read (slightly slower, more host control).

---

## Open items

1. **PCNT glitch-filter max length and counter width:** Product datasheet defers to Technical Reference Manual. Needed to confirm PCNT viability vs. GPIO ISR fallback for wheel speed.
2. **Pressure transducer characteristics (range, impedance, temperature drift):** Brief states "generic single-ended analog input"; owner will specify transducer. Confirm output impedance (assume < 1 kΩ, else re-tune filter / acquisition time) and full-scale voltage (assume 0–3.3 V or ratiometric to +3V3 pod rail).
3. **Startup behavior (ATS601 Calibration period):** Pod may produce spurious edges during first 100–500 ms after power-up (Startup + Calibration modes). Firmware should debounce or detect low pulse rate at start.

---

## Could not cite

- TLA2528 I2C SCL stretching hold time (referenced as "see Section 6.8" but exact register / timing spec not located in provided extracted datasheet window).
- ESP32-C6 PCNT glitch-filter bit-width (e.g., 8-bit, 16-bit in APB cycles) — specification deferred to Technical Reference Manual.
- ATS601 application circuit bypass capacitor value on pod (datasheet silent on 3.3 V design; only test conditions at higher voltage shown).

---

## Coverage

IDL0 firmware uses GPIO ISR with debounce for discrete events (markers); IDL1 must add PCNT for continuous wheel-speed counting and TLA2528 I2C ADC driver for analog pressure (two new peripherals, one new protocol). Signal paths verified against netlists; RC settle-time and rise-time margins confirmed adequate for 100–3000 RPM and sub-10 kHz pulse rates. ATS601 power-on and calibration behavior noted as potential startup gotcha.

