# Decisions — IDL1 program

Owner rulings on the 20 structural questions raised by the overnight runs of
2026-08-31. **This file is the authority.** Dispatches read it by path; nobody
pastes its contents into a prompt. Where a ruling contradicts a run's output,
this file wins and that output is corrected, not re-derived.

Answered 2026-09-02. Spec = `idl0-app/docs/IDL0_SPEC.md`, canonical there and
never edited from the firmware side.

Status key: **FINAL** — implement as written · **OPEN** — do not build on it yet.

---

## Pod IMU — LSM6DSV320X

### P1 + P6 — both accelerometers run. FINAL.

High-g is **mandatory**, not deferred. The DSV320X is quad-channel: the low-g
and high-g accelerometers are separate sensors that can be turned on and off
independently of each other, with different ODRs and power modes. Run both.

- Low-g path carries `IMUx_Accel*`. Valid `accel_range_g` = {2, 4, 8, 16}.
- High-g path at **±64 g** becomes new registry channels **24–29**
  (`IMU1_HgAccelX..Z`, `IMU2_HgAccelX..Z`), FIFO tag 0x1D.
- Single-axis high-g was considered and rejected: the FIFO tag carries the
  triplet, so one axis would mean polling output registers 34h–39h directly and
  abandoning FIFO timestamping. Not worth 4 bytes.

Two hard constraints the implementation must respect:

- **High-g has no batch data rate.** `XL_HG_BATCH_EN` is an enable bit, not a
  rate field — there is no `BDR_HG`. It batches at its ODR, which floors at
  **480 Hz**, and cannot be decimated in FIFO.
- **When high-g is enabled, the low-g accelerometer must be in high-performance
  or high-accuracy ODR mode** (datasheet Table 21). It constrains the low-g
  power mode; it is not free.

High-g peak tracking (FSM-driven, peak stored in FIFO under its own tag) is a
lower-bandwidth alternative but requires an FSM program. Not in scope now;
revisit after the streaming path logs cleanly.

### P2 — ODR set. FINAL, with one empirical value.

Valid `imu.sample_rate_hz` = the real DSV320X ladder:
**{7.5, 15, 30, 60, 120, 240, 480, 960, 1920, 3840, 7680}**.
No legacy-value mapping — see P3. Off-list values are rejected in validation,
not silently remapped.

**Every rate and range in this file is a shipped default, not a fixed value.**
All of them stay runtime-configurable over BLE config. Nothing here should be
implemented as a compile-time constant, and no dispatch should treat a default
as a constraint.

- Onboard IMU (SPI): default **960 Hz**.
- Pods (shared I2C): default **960 Hz — to be tried and measured.** Estimated
  bus load with high-g on both pods is ~89% of a 400 kHz bus (three sensors,
  7-byte FIFO words, 480 Hz high-g floor). That estimate is arithmetic, not
  measurement. Fallback default is 480 Hz.

Because the pod rate is empirical, two implementation requirements:

1. **Instrument pod FIFO overrun from day one.** Read and diag-log
   `FIFO_STATUS` overrun bits. A bus that cannot keep up loses samples silently
   and looks like gaps in the log, not an error.
2. **The rate is a config value, never a compile-time constant**, so dropping to
   480 is a config push rather than a reflash.

Subordinate and open: the current firmware runs this bus at 1 MHz although the
ESP32-C6 datasheet lists only 100/400 kbit/s, and its comment misattributes the
speed to the DSO32. The pods are now on cables with 4k7 pull-ups rather than
onboard traces. Measure at bring-up before relying on 1 MHz.

### P3 — gyro range. FINAL.

Valid `imu.gyro_range_dps` = **{250, 500, 1000, 2000, 4000}**. ±125 dps is gone.
**No legacy mapping** — expose the real values only. This principle applies to
the accel and ODR sets in P1 and P2 as well.

### P7 · H3 — pod I2C addressing. FINAL.

Front pod = **JP1 open = 0x6A = spec IMU1**. Rear pod = **JP1 closed = 0x6B =
spec IMU2**. Owner recollection (solder jumpered on the rear) agrees.
Mark the pod silkscreen. Firmware defines both addresses, confirms each with
WHO_AM_I at boot, and diag-logs an absent pod rather than guessing.

---

## Pod analog — TLA2528

### P5 — representation and ADC architecture. FINAL.

Fixed registry channels **20/21** — physically fixed, like the wheel inputs —
keeping `units`, `scale` and `offset` in config. `analog.channels[]` stays
reserved for the DNP spare header.

**Correction — read this before coding the ADC.** An earlier revision of this
file specified autonomous mode with `RECENT_CHx_LSB/MSB`, `STATS_EN` and
`CNVST`. **None of those exist on the TLA2528.** That text came from §7.3.6 of
TI's own datasheet, which opens "The **ADS7138** features…" — TI copy-pasted the
section from the fuller-featured sibling part and did not strip the features
this device lacks. Three separate conclusions in this project were built on that
paragraph before anyone checked it against the register map.

Verified against the PDF (`pdftotext -layout`, since docling dropped the tables):

- **Table 8, the complete register list, ends at 0x12** (`AUTO_SEQ_CH_SEL`), and
  states that every address not listed is reserved. There is no `RECENT_CHx`,
  no statistics register, no `CNVST`.
- **§7.4 has exactly three subsections**: power-up/reset, **manual mode**, and
  **auto-sequence mode**. There is no autonomous mode.
- **`OPMODE_CFG` (0x4) holds only `OSC_SEL` and `CLK_DIV[3:0]`** — no conversion
  -start bit, no mode enable.

So the architecture is the straightforward one:

> **Auto-sequence mode over AIN4/AIN5, host-initiated conversions, host reads
> the result.** OSR averaging is available and **clock stretching is the only
> mechanism for it** — the non-stretching escape does not exist on this part.

- **Pin `OSC_SEL=0` and keep `CLK_DIV` low.** This is now load-bearing rather
  than tidy: at `OSC_SEL=0` / `CLK_DIV=0000b` a cycle is 1 µs, so even OSR=128
  stretches only ~128 µs — about six bytes of bus airtime, tolerable inside a
  480 Hz poll window. At `OSC_SEL=1` / `CLK_DIV=1111b` the same average holds
  the bus for **786 ms**. Comment this in the driver so nobody optimises for
  power there later.
- Set `APPEND_STATUS` so the 4-bit channel ID rides with the data. Cheap
  insurance that the sequencer is where you think it is.
- **Store the 16-bit averaged result** when OSR is on, not a raw 12-bit count.
  The averaging module output is 16 bits wide.
- `analog.sample_rate_hz` means **how often firmware reads and converts**.

Ship with `OSR = 0` and expose the knob.

Valid `analog.sample_rate_hz` = {10, 50, 100, 200, 500} Hz, default 100. Ship
with `OSR = 0` and expose the knob.

---

## IDL1 board

### H1 — LEDs. FINAL.

- **Green** (D2, TLA2528 ch 7) — power and wireless connection.
- **Red** (D1, ch 6) — recording.
- Boot and failure states use blink patterns. Exact patterns are the owner's
  call and are not blocking.
- Keep the XIAO onboard LED (GPIO15, active-low) as `IDL1_PIN_LED_ONBOARD`.

Consequence to design around: **both board LEDs sit behind I2C**, so nothing can
indicate state until the bus and the TLA2528 are up. The onboard LED covers that
window and hard faults.

### H2 — TLA2528 address. FINAL — resolved, and it is the bad case.

**ADDR ties to GND through NT1.** Settled from the exported netlist, not from
geometry: `Net-(U4-ADDR)` carries U4 pin 11 and NT1 pin 2; NT1 pin 1 is on `GND`.
See `findings/2026-09-02-h2-addr/addr-resolution.md`.

The hw-delta run was correct. The pod-delta run's 0x17 was wrong, and so was the
earlier reading of this file — 0x17 is `R1 = 0 Ω, R2 = DNP`, i.e. 0 Ω to *DECAP*,
which is not what this board does.

Leg orientation is confirmed from **Figure 23**, readable via `pdftotext -layout`
on `references/source/tla2528.pdf` (docling dropped it as an image). The figure
stacks DECAP&nbsp;→ R1 → ADDR → R2, so **R1 is the DECAP leg and R2 is the GND
leg**. The board is therefore `R1 = DNP, R2 = 0 Ω`, and Table 2's R2 column
holds only DNP / 11k / 33k / 100k.

**Calibrate the claim carefully.** Nothing in the datasheet forbids or warns
against this. §7.3.5 says only "connect a resistor to this pin from DECAP pin or
GND", then gives the table. 0 Ω on R2 is *absent from the table*, not
prohibited — and 0 Ω on R1 is an explicitly valid row (0x17), so TI plainly
tolerates a short on that pin. The correct statement is that **the resulting
address is unspecified, not that the board is defective.**

**No automatic address search in firmware.** Owner will resolve the address
manually at bring-up. Firmware probes exactly one expected address and reports
clearly; it does not sweep the range.

- **Expected address: `0x10`** — the documented `R1 = DNP, R2 = DNP` row, which
  is what the board becomes once NT1 is opened. Keep it as a single named
  constant that is trivial to change while testing candidates.
- Probe it at boot, confirm with a device-ID read, and **diag-log a distinct,
  loud failure** if it does not ACK. Silence or a hang is the wrong behaviour
  here.
- **Boot must survive the ADC not answering.** Both board LEDs are TLA2528 GPOs
  (H1), so a non-responding ADC means no board LED indication at all — the
  onboard GPIO15 LED is the only signal in that state. It must still work.

**Bring-up caveat on opening NT1.** NT1 is `Device:NetTie_2` with `(in_bom no)`,
`(on_board yes)` — it is a **copper net tie, not a fitted resistor**. It cannot
be depopulated; opening it means cutting the copper between its two pads. That
is a knife job on the board, not a rework, and it is not cleanly reversible.

**Next spin: replace NT1 with a real 0603 pad pair to GND.** Fit 11k for a
documented 0x11, or leave it DNP for 0x10. Either way the address becomes a
populate-or-not decision instead of a scalpel.

### H4 — pins header. FINAL.

New `main/pins_idl1.h`:

- `IDL1_PIN_*` for real GPIOs, typed `gpio_num_t`
- `IDL1_TLA_CH_*` for TLA2528 channels, plain ints
- Kconfig choice `CONFIG_IDL_BOARD_IDL0` / `CONFIG_IDL_BOARD_IDL1`
- `pins.h` becomes a thin `#if` that includes exactly one board header
- Drivers include `pins.h` only

The prefix split is load-bearing: TLA2528 channel 7 and ESP32 GPIO7 (`SD_CS`)
are a live collision, and a `GPIO_NUM_7` LED macro would chip-select the SD card.

---

## IDL0 firmware — audit rulings

### A1 — config commands during a session. FINAL.

`CONFIG_BEGIN` / `CONFIG_COMMIT` / `CONFIG_READ_BEGIN` write the **firmware's**
`idl0_config.json`. Refuse all three with `IDL0_ACK_MUTEX_REFUSED` (0x03) while
`idl0_session_is_running()`. Spec §7.2 gains one line in its mutex list.
Unblocks triage row T01 (Blocker).

### A2 — first-fix rename. FINAL — reject the deferral.

**Keep the rename at first fix.** Deferring it to session close would leave the
file tmp-named after a power loss, which is the outcome that matters most. The
handle-leak race is fixed independently by **T02** (close the stray handle),
already filed as a ready dispatch. Both problems solved, no spec change.
Add a power-loss-mid-session test.

### A3 — 1 Hz BLE status publisher. FINAL.

Exempt. The control plane is not the logging path. Record the constraint in
CLAUDE.md §2 and spec §1 using the owner's clarification: the rule is **zero
*data* processing**, not zero activity. Future audits then stop re-flagging it.

### A4 — NMEA decode. FINAL.

Required by §5.6, not forbidden. Replacing `atof` and double math with integer
parsing in `gps_driver.c` is a contained follow-up, verified by
`test/test_gps_nmea.c`.

**Backlog, not this port:** parse satellite count and other GNSS diagnostics and
publish them over BLE.

### A5 — zero-filled IMU record. FINAL.

One zero-filled `IMU_SAMPLE` per poll cycle per IMU that was active at session
start and is now in ERROR, timestamped at the poll. Spec §4.5 gains that sentence.

### A6 — unparsed §8 config fields. FINAL.

**Omit them for now**; add individually as needed. They are not counted as
firmware gaps and no dispatch should treat them as such. Note for whenever they
do land: `gps.dynamic_model` and `gps.nmea_sentences` genuinely change GPS
behaviour (UBX/PUBX config at init). The rest are app-side or deferred features.

### A7 — SD full and storage reporting. FINAL, narrowed.

**Firmware scope is now just: report card capacity and usage over BLE status.**
The low-storage warning, the stop-versus-overwrite policy and any user-facing
choice are app-side work for later. Do not build policy machinery into firmware.

That still leaves a Blocker: on SD FULL or ERROR the firmware today neither
stops nor notifies, and §10.2 promises both. Deferring the *policy* does not
remove the need for *a* behaviour. Take the conservative one, which is also what
§10.2 already says:

> On SD FULL or ERROR, stop the session and report it. `writer_task` sets a
> `volatile` fault flag on the first append failure while state is FULL or
> ERROR; the existing 1 Hz status timer checks it, calls `idl0_session_stop()`,
> then publishes `SD: FULL|ERROR` with `Logging: STOPPED`. No new task.

Overwrite-oldest becomes a later option layered on that, not a replacement for
it. **T04 and T38 are unblocked and may be dispatched** on the mechanism above.

### A8 — header `imu_count`. FINAL.

**Remove it.** It predates the per-IMU fields and is redundant against
`imu.imuN.enabled` plus the channel mask. Binary format changes are acceptable
here.

Not lockstep. The firmware does not depend on the app: make the format change
here, **bump the format version**, and let `idl-rs` catch up when the app is
ported. The version field is what makes firmware-first safe — an older app can
detect a newer file rather than misparsing it. Verify a version field exists
before landing this; if none does, that is a prerequisite, not an optional extra.

### A9 — BLE notify ordering. FINAL.

Acceptable. Document in §7.2 that a status notify reflecting a command may
arrive before that command's ATT write response, and that the app must key on
notify content rather than ordering.

### A10 — SPI2 shared by SD and IMU0. FINAL.

Accept on IDL0, bounded by T30's burst cap and T31's re-init so a stall stays
recoverable. On IDL1 the question largely dissolves: the pods are on I2C and
only the onboard IMU shares SPI with the SD card. Carry it as an IDL1 bring-up
check, not an IDL0 fix.

### P4 — spec hardware sections. FINAL.

Add a **"§3.x IDL1 hardware deltas"** section in idl0-app describing the current
hardware accurately. Do not rewrite §3.2–3.5 in place — IDL0 units must stay
documented.

---

## Outstanding

| Item | What it blocks | Next step |
|---|---|---|
| **P2** pod default of 960 Hz | nothing — 480 is the fallback | bring-up measurement |
| **A8** format version field | the `imu_count` removal | confirm one exists in `idl0_format.h` |
| **H2** next-spin resistor | nothing now — scan covers the board in hand | 11k to GND on NT1 |

A7 is no longer blocking: T04 and T38 may be dispatched on the stop-and-report
mechanism above. Everything else is FINAL and safe to dispatch from.

## Out of scope for firmware

The IDL1 app is being carried forward separately on `idl0-app` and ported when
boards are closer. **Firmware does not depend on it.** No dispatch from this
file should wait on, or make changes to, the app or `idl-rs`.

---

## Q8 / Q9 — config field names (ruled by orchestrator, 2026-09-02)

Contained blast radius: names only, nothing coded against them yet, app port is
months out. Overridable until firmware ships. Chosen to match existing §8 style:

- **Q8 high-g:** `imu.hg_range_g` (valid {32, 64, 128, 256, 320}, default 64)
  and `imu.hg_sample_rate_hz` (valid = DSV320X high-g ODR ladder, default 480,
  floor 480 — the hardware floor, not a policy).
- **Q9 ADC oversampling:** `analog.osr` (valid {0, 2, 4, 8, 16, 32, 64, 128},
  default 0). When nonzero the stored value is the 16-bit average.

All are runtime-configurable defaults per the standing rule.

---

## Bring-up week — rulings of 2026-09-22 (boards arrived)

### F1 — repo. FINAL.
`saucyeng/idl1-firmware`, **private**, forked with full history from
`idl0-firmware` main at `f4f26c6`. `idl0-firmware` becomes the frozen IDL0
firmware (bug fixes only).

### F2 — IDL1-only. FINAL. Supersedes H4's Kconfig choice.
No `CONFIG_IDL_BOARD_*`, no `pins_idl0.h`, no neutral `IDL_PIN_*` aliases, no
DSO32 driver. H4's namespace split survives intact: `IDL1_PIN_*` (gpio_num_t),
`IDL1_TLA_CH_*` and `IDL1_I2C_ADDR_*` (plain ints). `pins.h` holds the IDL1 map
directly. Reason: phase 2 replaces the DSO32 driver with the DSV320X; keeping
IDL0 building would mean carrying two IMU drivers for a board that already has
its own frozen firmware.

### F3 — binary format stays schema v3 all week. FINAL. Defers A8.
New sensors enter through the channel registry only (high-g 24–29, analog
20/21, wheel 18/19), which the idl0 app skips safely per spec §5.2. A8
(`imu_count` removal + schema bump) lands with the idl1-app format work. Wire
identifiers the idl0 app keys on (BLE UUIDs, advertised name pattern, "IDL0"
magic, status-line format) are unchanged through phase 3.

### F4 — phases. FINAL.
1. **Power on** — every chip answers; boot self-test report; OTA board guard.
2. **Port old features** — DSV320X low-g/gyro on all three IMUs, GPS, SD,
   BLE, WiFi, OTA, HRM, markers, LED roles (H1). Reachable from the idl0 app.
3. **New features** — high-g 24–29, TLA2528 analog 20/21, wheel 18/19,
   battery sense, user button, config fields Q8/Q9.
Plan: `plans/2026-09-22-idl1-bringup-phases.md`.

### F5 — OTA board guard. FINAL (orchestrator, safety).
The idl0 app's firmware catalog pulls `saucyeng/idl0-firmware` releases. An
IDL0 image on IDL1 hardware drives GPIO2 (PWR_EN) as an IMU CS and GPIO23 (SCL)
as SD CS. IDL1 firmware rejects any OTA image whose `esp_app_desc_t.project_name`
is not `idl1_firmware`, checked on the first chunk before anything is written.
