# Questions — run 2026-08-31-pod-delta

Structural items the run did not decide. Each has a recommended answer and the
assumption the outputs were built on. Fill in the blank; outputs listed under
"Affected outputs" are marked PROVISIONAL until you do.

Spec = `idl0-app/docs/IDL0_SPEC.md` (canonical, never changed from the firmware side).

### Q1 — Which DSV320X accelerometer path carries the IMU accel channels: low-g (max ±16 g) or high-g (±32…±320 g)?
- **Where:** Spec §3.2 ("±32g accel"), §8 `imu.accel_range_g` default 32, §5.2 registry scale `accel_range_g / 32768`; `regs-ctrl.md` rows "Accelerometer full-scale (low-g)" and "(high-g)".
- **Context:** The DSO32 had ±4/8/16/32 g on one sensor. The DSV320X has two accelerometers: the low-g path tops out at ±16 g (CTRL8 FS_XL) and the high-g path starts at ±32 g (CTRL1_XL_HG, FS_XL_HG 000=±32 g) with its own ODR set (480 Hz–7.68 kHz), its own output registers (34h–39h), its own FIFO tag (0x1D) and self-test. A config of `accel_range_g: 32` cannot be honoured by the low-g path, and the high-g path cannot go below ±32 g. This decides which registers the driver reads for the `IMUx_Accel*` channels.
- **Blast radius:** structural — spec §8 valid values, §3.2, and the registry scale semantics.
- **Recommended answer:** Low-g path for `IMUx_Accel*`, valid `accel_range_g` ∈ {2, 4, 8, 16}; map a legacy config value of 32 to 16 with a diag-log warning. Treat the high-g path as a *separate future channel set* (see Q6), not as a range setting.
- **Proceeded on:** Low-g path; `accel_range_g` 32 → 16.
- **Affected outputs:** PORT-CHECKLIST.md items tagged Q1.
- **Your answer:** ______________________________________________

### Q2 — New IMU ODR set: what are the valid `imu.sample_rate_hz` values on IDL1, and does 833 Hz become 960 Hz?
- **Where:** Spec §8 "Valid `sample_rate_hz` values" table (12.5/26/52/104/208/416/833/1666 Hz high-perf); `regs-ctrl.md` "Accelerometer ODR encoding" and "Gyroscope ODR encoding".
- **Context:** DSV320X ODR codes are 7.5/15/30/60/120/240/480/960/1920/3840/7680 Hz (plus 1.875 Hz accel LP). None of the DSO32 values exist. §8 says off-list values are undefined (firmware maps directly to the ODR register), so the app UI's discrete list and the firmware's lookup table must both change. FIFO batch-rate codes changed the same way (`regs-fifo.md`).
- **Blast radius:** structural — §8 table and the app's picker.
- **Recommended answer:** New valid list {7.5, 15, 30, 60, 120, 240, 480, 960, 1920, 3840, 7680}; default 960 for the onboard (SPI) IMU, **≤ 480 for the two pods** — the shared 400 kHz I2C bus is ≈ 63 % busy at 960 Hz with two pods (checklist A9/D4). Firmware maps any legacy value to the nearest valid ODR and logs it (the current fallback 104 Hz at `imu_task.c:77` → 120); registry `sample_rate_hz` (u16) carries the actual value so old logs stay self-describing.
- **Proceeded on:** Nearest-ODR mapping; default 960 Hz onboard / 480 Hz pods.
- **Affected outputs:** PORT-CHECKLIST.md items tagged Q2.
- **Your answer:** ______________________________________________

### Q3 — Gyro range set: ±125 dps is gone, ±4000 dps is new. Update §8 to {250, 500, 1000, 2000, 4000}?
- **Where:** Spec §8 "gyro range (±125–2000 dps)"; `regs-ctrl.md` "Gyroscope full-scale" (FS_G[2:0] in CTRL6, 000 reserved) and "±125 dps ... removed".
- **Context:** Small, but it is a §8 change and the registry scale `gyro_range_dps / 32768` must receive a value the chip actually applied.
- **Blast radius:** structural — §8.
- **Recommended answer:** Valid {250, 500, 1000, 2000, 4000}; legacy 125 → 250 with a warning.
- **Proceeded on:** That mapping.
- **Affected outputs:** PORT-CHECKLIST.md items tagged Q3.
- **Your answer:** ______________________________________________

### Q4 — Spec hardware sections §3.2 / §3.4 / §3.5 describe IDL0. Add an IDL1 hardware addendum or revise in place?
- **Where:** Spec §3.2 "LSM6DSO32TR ×3", §3.4 "ESP32-C6 ADC, 12-bit, 0–3.3 V … No input protection", §3.5 "Interrupt-driven, timestamped per pulse (not polled)".
- **Context:** On IDL1 all three IMUs are LSM6DSV320X (onboard on SPI, pods on I2C); the pod analog lines are read by a TLA2528 I2C ADC (12-bit, 0–3.3 V) behind a TVS + 33 R / 100 nF front end — so "no input protection" is no longer true; wheel inputs have a 4k7 pull-up + 10 nF filter and could use PCNT. None of this changes the log format, but the spec would misdescribe the shipping hardware.
- **Blast radius:** structural — spec sections.
- **Recommended answer:** Add "§3.x IDL1 hardware deltas" in idl0-app rather than rewriting §3.2–3.5, so IDL0 units stay documented.
- **Proceeded on:** No spec text written; checklist references the netlist-derived facts instead.
- **Affected outputs:** none PROVISIONAL (documentation only).
- **Your answer:** ______________________________________________

### Q5 — How is the pod analog input represented: fixed registry channels 20/21 (`PressureFront/Rear`, u16 @ 100 Hz, "bar") or the generic `analog.channels[]` list (with `adc_pin` now meaning a TLA2528 AIN index)?
- **Where:** Spec §5.2 rows 20/21; §8 `analog.channels[]` (`adc_pin`, `units`, `scale`, `offset`) and the note "`analog.sample_rate_hz` — **Not yet defined.** ADC chip and valid rate set unspecified — define before implementing analog parsing."
- **Context:** The spec itself says the ADC chip and rate set must be defined before analog parsing is implemented. On IDL1 the chip is the TLA2528 (front pod → AIN5, rear pod → AIN4), 12-bit, 0–3.3 V, address 0x17, shared I2C bus with both pod IMUs. Both representations already exist in the spec; they overlap. The owner's instruction is "prep it to do the basic analog input" with the transducer undecided — so the firmware needs raw counts → u16 with scale/offset from config, nothing more.
- **Blast radius:** structural — §5.2 / §8 semantics.
- **Recommended answer:** Use fixed channels 20/21 for the two pod inputs (they are physically fixed, like the wheel inputs), keep `units`/`scale`/`offset` for them in config, and define `analog.sample_rate_hz` valid set as {10, 50, 100, 200, 500} Hz for TLA2528 manual-mode polling on the shared bus. Leave `analog.channels[]` for the DNP spare header (AIN2/AIN3) later. Store raw 12-bit counts (0–4095) as u16; scale/offset convert to bar in the app.
- **Proceeded on:** Channels 20/21, raw counts, 100 Hz default (spec's existing rate for 20/21).
- **Affected outputs:** PORT-CHECKLIST.md items tagged Q5 (PROVISIONAL).
- **Your answer:** ______________________________________________

### Q6 — Record the DSV320X high-g accelerometer at all (as registry channels 24+ via 0x03 records)?
- **Where:** Spec §5.2 "Future sensors (no format change needed): 24+ anything"; `regs-fifo.md` tag 0x1D; `regs-ctrl.md` high-g rows.
- **Context:** The spec allows new registry channels without a format change, so this is not a format question — it is a product question (do you want ±32 g+ shock data from the unsprung pods, at what ODR, at what I2C bus cost: +6 bytes per IMU per sample).
- **Blast radius:** structural — what later tasks build on (channel IDs, bus budget).
- **Recommended answer:** Not in the first pod firmware. Reserve channel IDs 24–29 (`IMU1_HgAccelX..Z`, `IMU2_HgAccelX..Z`) on paper; implement after the low-g path is logging cleanly.
- **Proceeded on:** Not implemented; IDs noted only.
- **Affected outputs:** none PROVISIONAL.
- **Your answer:** ______________________________________________

### Q7 — Pod I2C address convention: which pod is 0x6A and which is 0x6B?
- **Where:** Pod schematic JP1 + R1 10K on IMU1 SDO/TA0 (`breakout board-facts` §3); `regs-i2c.md` "Address when LSB pin = 0/1"; main-board I2C fan-out to J4 (rear) / J5 (front) / J6.
- **Context:** Both pods are the same PCB on one I2C bus, so one must have JP1 bridged (SDO/TA0 = VDDIO → 0x6B) and the other left open (R1 pulls SDO/TA0 to GND → 0x6A). The firmware needs a fixed rule to know which address is "front" (spec IMU1) and which is "rear" (spec IMU2). This is an assembly convention with firmware consequences.
- **Blast radius:** structural — fixes the IMU index ↔ address mapping that everything downstream relies on.
- **Recommended answer:** Front pod = JP1 open = 0x6A = spec IMU1; rear pod = JP1 bridged = 0x6B = spec IMU2. Mark the pod silkscreen/label accordingly. Firmware probes both addresses at boot and diag-logs which it found.
- **Proceeded on:** That mapping.
- **Affected outputs:** PORT-CHECKLIST.md items tagged Q7 (PROVISIONAL).
- **Your answer:** ______________________________________________
