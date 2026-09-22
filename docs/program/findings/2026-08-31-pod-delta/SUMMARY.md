# SUMMARY — run 2026-08-31-pod-delta

Plan: `idl1-program/02-pod-sensor-delta.md`. Read-only; no repo source touched. One derived artifact was created (docling parse of the 500 psi PDF — it came out empty, see below).

## What ran

| Pass | Model | Dispatches | Output |
|---|---|---|---|
| 1 register delta | haiku ×7 (+1 audit) | identity-boot, ctrl, fifo, interrupts, spi, **i2c** (added — the pod is on I2C, not SPI), selftest; then a verbatim bit-map audit | `regs-*.md`, `regs-bitmap.md` |
| 2 driver impact | sonnet ×1 | IDL0 IMU path vs DSV320X, file:line | `driver-impact.md` |
| 3 new sensors | haiku ×1 (+1 transcription) | ATS601LSG + generic analog input; then a verbatim TLA2528 register/opcode sheet | `new-sensors.md`, `tla2528-regmap.md` |
| synthesis | orchestrator | — | `PORT-CHECKLIST.md`, `questions.md` (7), `ledger.md` |

Scope change from the owner mid-run: "don't worry about the exact pressure sensor yet — prep basic analog input." Applied to Pass 3 and the checklist.

## What each output contains

- **`PORT-CHECKLIST.md`** — the deliverable. A-reg: 30 register-level mapping rows (old value → new value, cited). A-code: 15 code changes with `file:line`. B: wheel speed (7). C: analog input via TLA2528 (8). D: power sequencing / bus ownership (4). E: map of items to spec questions. Every row sized transcription (41) or judgment (16). Recommended order: vendor driver → dedupe SPI/I2C copies → transcription batch → bus/pins/addresses → boot → wheel → analog.
- **`questions.md`** — 7 structural items with recommended answers and a blank each. Q1 accel path/range, Q2 ODR set, Q3 gyro set, Q4 spec §3 hardware text, Q5 analog representation + rate set, Q6 high-g channels, Q7 pod address convention.
- **`driver-impact.md`** — verdict: `stmdev_ctx_t` adapter swaps unchanged; six enum helpers must be rewritten; no new public interface; SPI and I2C files duplicate 14 pieces; data-ready already polls FIFO level (no INT dependence); two I2C addresses already supported.
- **`regs-bitmap.md`** — use its DSV320X columns for bit positions. Three haiku bit-position miscounts were found in the delta files and annotated at the bottom of `regs-spi.md`, `regs-interrupts.md`, `regs-bitmap.md`.
- **`new-sensors.md`** — ATS601 (open-drain, 3.3–24 V, ≤ 1 ms power-up, calibration on first 3–9 edges), full signal paths pod → main board with refdes, RC checks. Its TLA2528 register table is superseded by `tla2528-regmap.md`.
- **`ledger.md`** — decisions, verified facts, review notes. Timestamps written before 21:30 ran ~50 min fast; file mtimes are authoritative.

## Three things worth attention in the morning

1. **Q1 gates the IMU port.** The config default `accel_range_g: 32` cannot be honoured on the DSV320X low-g accelerometer (max ±16 g); ±32 g exists only on the separate high-g sensor with its own registers, ODR set and FIFO tag. Recommended: low-g path, 32 → 16, high-g deferred. Q2/Q3 (new ODR and gyro sets) are the same shape of decision and touch spec §8.
2. **The pod I2C bus is the bottleneck, and the host is the limit.** The ESP32-C6 datasheet lists its I2C controller at 100/400 kbit/s only; the current firmware runs the bus at 1 MHz (its comment wrongly credits the DSO32). Two pods at 960 Hz ≈ 63 % of the poll window at 400 kHz; 480 Hz ≈ 33 %. Bus speed and pod ODR must be chosen together (checklist A9, A14; questions Q2). Cable capacitance with 4k7 pull-ups argues for 400 kHz.
3. **The pod analog line is read by the TLA2528, not the ESP32-C6 ADC**, and spec §8 says `analog.sample_rate_hz` must be defined before analog parsing is implemented (Q5). The TLA2528 is opcode-addressed (0x08 write / 0x10 read / plain address+R for conversion data); its 12-bit-in-two-bytes packing is an image in the datasheet that the extraction could not text-ify — read Figure 25 from `references/source/tla2528.pdf` before coding the shift. The same chip drives both status LEDs (GPIO6/7), so its PIN_CFG write is shared with `led_status`.

Also: the 500 psi pressure-sensor PDF is image-only — `parse.py --ocr` when the transducer is chosen. No IMU INT lines exist anywhere on IDL1; all three IMUs are polled. `PWR_EN` (GPIO2) gates the switched +3V3 for every sensor, and IDL0's `pins.h` would drive that pin as an IMU chip-select — regenerate pins from the IDL1 netlist (run 03).
