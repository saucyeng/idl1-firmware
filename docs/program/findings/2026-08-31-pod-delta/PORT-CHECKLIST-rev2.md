# PORT-CHECKLIST-rev2 — IDL1 pod sensors: LSM6DSV320X + ATS601LSG + analog input

Run `2026-08-31-pod-delta`, revised against `idl1-program/decisions.md` (answered
2026-09-02). **`decisions.md` is the authority; where it contradicts this
document's rev1 reasoning, rev1 is corrected here, not re-derived.** No
firmware written — planning document only. Owner scope note carried over from
rev1 (2026-08-31 21:20): the pressure transducer is undecided — the analog
channel is prepared as a generic single-ended input.

## What changed from rev1

1. **High-g is now mandatory, not deferred (decisions.md P1 + P6).** Both
   LSM6DSV320X accelerometers run simultaneously. Low-g still carries
   `IMUx_Accel*` (≤ ±16 g); a new high-g path at ±32–±320 g (default ±64 g)
   becomes registry channels **24–29**, FIFO tag **0x1D**. Rev1's Part A-reg
   R13 flips from "not used unless Q1/Q6" to real, mandatory work. New A-code
   rows **A16–A21** and new A-reg rows **R31–R32** implement it. Two hard
   constraints carried in verbatim: high-g has no FIFO batch-rate field
   (`XL_HG_BATCH_EN` is enable-only; it batches at its ODR, which floors at
   **480 Hz** and cannot be decimated), and enabling high-g forces the low-g
   accelerometer into high-performance or high-accuracy-ODR mode (A5, A18).
2. **The TLA2528 analog architecture changed (decisions.md P5).** Rev1's
   manual-mode, firmware-triggered-conversion design (old C2/C4) is replaced
   by **autonomous mode + auto-sequence over AIN4/AIN5 + OSR**, reading
   `RECENT_CHx_LSB/MSB` on demand — the ADC free-runs and firmware just reads
   the latched result. Part C is restructured (12 rows, C1–C12) around this.
   `APPEND_STATUS` is set, `OSC_SEL=0`/`CLK_DIV` low is pinned and commented,
   and firmware stores the chip's **16-bit averaged result**, not a raw 12-bit
   count. **Citation gap carried forward, flagged, not invented:** the
   `RECENT_CHx_LSB/MSB` register address does not appear in this run's
   `tla2528-regmap.md` extraction (it stops at register 0x12); decisions.md's
   architecture ruling is FINAL regardless, but the address itself must come
   from `references/source/tla2528.pdf` (or a fresh docling pass) before
   coding — see C3.
3. **No legacy value mapping, anywhere (decisions.md P2 + P3, explicit
   principle).** Every rev1 row that mapped an old config value to a new one
   (32 g → 16 g, 833 Hz → nearest ODR, 125 dps → 250 dps) is rewritten: expose
   only the real device values, reject off-list values in validation. This
   removes the **PROVISIONAL — Q1/Q2/Q3** tags from A4, A8, A14 and resolves
   Q1–Q3 in Part E.
4. **Every rate and range is a runtime-configurable default, never a
   constant (decisions.md, general principle under P2).** A14's ODR fallback
   and the new high-g ODR/FS defaults (A16) are reworded so no dispatch bakes
   a rate or range in as a `#define` divorced from config.
5. **decisions.md A7 (SD full/capacity reporting over BLE) does not touch
   this document.** That ruling narrows *idl0-firmware's* storage-policy scope
   generally; this checklist covers pod IMU/wheel/analog/power-sequencing
   only and never referenced SD storage. No rows changed for it — noted so
   its absence here isn't mistaken for an oversight.
6. **Q7 (pod address convention) resolves FINAL (decisions.md P7),** exactly
   as rev1's recommended answer: front = JP1 open = 0x6A = IMU1, rear = JP1
   closed = 0x6B = IMU2. A11 and D3 drop their **PROVISIONAL** tag.
7. **Bus budget recomputed (this run's own arithmetic, not decisions.md's
   headline number)**, folding the mandatory high-g stream into the shared
   pod I2C bus load. Verdict: **~80% duty cycle at 960 Hz for two pods — not
   comfortably viable; 480 Hz should be the shipped default until bring-up
   measurement says otherwise.** This is the same conclusion decisions.md
   reaches from its own ~89% estimate (arithmetic, not measurement); the two
   numbers differ because of different transaction-grouping assumptions, not
   because either overrides the other. See D4.
8. Part B (wheel speed, ATS601LSG) and Part D1/D2/D3 (boot order, bus
   ownership, address probe) are **unaffected** by every owner decision above
   and are carried forward unedited except D3's PROVISIONAL tag (point 6).
9. Part E's question log is updated: **Q1, Q2, Q3, Q5, Q6, Q7 are now
   RESOLVED** (cited to their `decisions.md` ruling); Q4 is resolved as
   documentation-only (P4). Two new logged, not-yet-owner-decided items are
   appended — **Q8/Q9**, the config field *names* for the new high-g and OSR
   knobs, an implementation detail decisions.md mandates the existence of but
   doesn't spell the names of.

Item-count and row-sizing changes are summarized at the end of each part and
totalled at the bottom of this document, replacing rev1's single tally line.

---

## How to use this

Every row is sized **transcription** (the change is fully specified by a
cited row in this run's findings, or by `decisions.md` directly — dispatch to
a cheap model) or **judgment** (a design choice remains — mid tier or a
human). No row in this revision carries a **PROVISIONAL** tag; every owner
question rev1 raised is resolved in `decisions.md` (Part E logs the
resolutions and the two still-open field-naming details).

Facts behind the rows, in order of authority:
0. `idl1-program/decisions.md` — **owner rulings; wins over every source below
   where they conflict.**
1. `regs-bitmap.md` — verbatim DSV320X register bit rows with `lsm6dsv320x.md`
   line cites (DSO32 columns unreliable; use
   `components/lsm6dso32x_STdC/lsm6dso32x_reg.h` for the old part). **Caution
   carried forward and sharpened:** this file's own "decoded" `FS_XL_HG` /
   `ODR_XL_HG` value tables (its lines ~166–187) do not match `regs-ctrl.md`'s
   high-g rows or `decisions.md` P1 (they show ±8 g…±24 g and
   12.5 Hz…1667 Hz — the old DSO32 ODR ladder, misattributed). Use
   `regs-ctrl.md`'s high-g rows for values; `regs-bitmap.md` only for register
   addresses/bit positions (:3201, :3183, :1857 verified against
   `lsm6dsv320x.md` directly).
2. `regs-identity-boot.md`, `regs-ctrl.md`, `regs-fifo.md`, `regs-interrupts.md`,
   `regs-spi.md`, `regs-i2c.md`, `regs-selftest.md` — the DSO32→DSV320X delta,
   page-cited. **Their bit *positions* are not to be trusted** (three
   miscounts found and annotated at the bottom of the affected files);
   addresses, defaults, encodings and values are.
3. `driver-impact.md` — every `file:line` in the IDL0 IMU path that breaks,
   the STdC swap verdict, the duplication map, the I2C bus-budget model this
   revision's D4 reuses.
4. `new-sensors.md` + `tla2528-regmap.md` — ATS601, signal paths, TLA2528
   registers/opcodes/timing (the register-sequence table inside
   `new-sensors.md` is superseded by `tla2528-regmap.md`). **Gap carried
   forward:** neither file's extraction contains `RECENT_CHx_LSB/MSB` or
   Figure 25's bit-packing — see C3.
5. Board wiring: `idl1-hardware/docs/datasheet/breakout/data/board-facts.md`
   (pod) and `idl1-hardware/docs/datasheet/idl1/data/board-facts.md` (main
   board) — netlist-derived.

Verified wiring the whole list rests on: the pod's LSM6DSV320X is on **I2C**
(CS tied to VDDIO, INT1/INT2 unconnected, address LSB by JP1/R1); IDL1's
onboard IMU is **also** an LSM6DSV320X on SPI (CS GPIO21, bus shared with
microSD); the DSO32 does not exist on IDL1. ATS601 OUT → XIAO **GPIO0**
(front) / **GPIO1** (rear) with 4k7 pull-up + 10 nF + 330 R on the main board.
Pod analog → **TLA2528** AIN5 (front) / AIN4 (rear), I2C 0x17, same bus as the
pod IMUs. `PWR_EN` = GPIO2 gates the switched +3V3 that feeds all of it. The
ESP32-C6 I2C controller supports **100 / 400 kbit/s only** (datasheet
§4.2.1.3) — decisions.md P2 flags the pods are now on cables with 4k7
pull-ups rather than onboard traces, and to measure at bring-up before
relying on any figure above 400 kHz.

Recommended execution order (unchanged in shape from rev1; A16–A21 slot in
after A15 since high-g reuses A7's dedupe target):
**A2 → A7 → (A1, A3, A4, A5, A6, A8, A12, A13, A14, A16, A17, A18) → A9, A10,
A11 → A19, A20, A21 → D1–D3 → B → C.**
A7 (dedupe) first means every later IMU edit — including the new high-g
writes — is made once, not twice.

---

## Part A — IMU: LSM6DSO32 → LSM6DSV320X in `idl0-firmware/main/` and `components/`

### A-code — What changes in the code (from `driver-impact.md`; every line verified by that pass; A16–A21 new this revision)

| # | Change | Where | Size | Spec / value |
|---|---|---|---|---|
| A1 | WHO_AM_I constant 0x6C → **0x73** and the log string that quotes it. | `imu_driver.c:46`, `:340-342`; `imu_driver_i2c.c:32`, `:271-277`; doc `imu_driver.h:56-57` | transcription | A-reg R1 |
| A2 | Vendor ST's DSV320X driver as `components/lsm6dsv320x_STdC/` (`lsm6dsv320x_reg.{c,h}`, `CMakeLists.txt`, `idf_component.yml`, `LICENSE`, `README.md` with the pulled tag — same layout and README wording as `lsm6dso32x_STdC/`; source `github.com/STMicroelectronics/lsm6dsv320x-pid`). The `stmdev_ctx_t` adapter (`platform_read/write/mdelay`) is **identical** — verified by Pass 2 from the upstream header — so `imu_driver.c:66,331-335` and `imu_driver_i2c.c:47,262-266` need no change. Decide whether `lsm6dso32x_STdC/` stays for IDL0 builds (board-select in CMake/Kconfig) or is deleted. | `components/` | transcription (vendoring) + judgment (keep-both vs replace, one line) | driver-impact §STdC API surface |
| A3 | Rename every `lsm6dso32x_*` symbol used in `main/` to `lsm6dsv320x_*` — 18 symbols listed in `driver-impact.md` §STdC API surface with their call sites. The new names are *reasoned by ST convention, unverified*: grep each against the vendored `lsm6dsv320x_reg.h` from A2 before applying; any that differ are a one-line fix each. | all call sites in that table | transcription (after A2) | driver-impact §STdC API surface |
| A4 | Rewrite the six conversion helpers `xl_odr_enum`, `gy_odr_enum`, `xl_bdr_enum`, `gy_bdr_enum`, `xl_fs_enum`, `gy_fs_enum` against the new tables — Hz set {7.5 … 7680} (R8/R10/R20), accel FS {2, 4, 8, 16} g in CTRL8 (R12), gyro FS {250 … 4000} dps in CTRL6 (R14). Remove the 32 g and 125 dps cases; **no legacy-value mapping (decisions.md P2/P3, FINAL — reverses rev1's Q1–Q3 recommendation).** The `default:` arm must not silently pick a range or a nearest rate — reject the off-list value at config-validation time (`CONFIG_COMMIT`, before it ever reaches these helpers), log it, and leave the prior valid setting in place. | `imu_driver.c:139-220`; `imu_driver_i2c.c:97-173` (once, after A7) | transcription | A-reg R8, R10, R12, R14, R20; decisions.md P2, P3 |
| A5 | Power-mode calls: `xl_power_mode_set(HIGH_PERFORMANCE_MD / LOW_NORMAL_POWER_MD)` and `gy_power_mode_set(GY_HIGH_PERFORMANCE / GY_NORMAL)` become writes of `OP_MODE_XL` (CTRL1[6:4]) and `OP_MODE_G` (CTRL2[6:4]): `high_performance=true` → 000; `false` → **111 "normal"** for accel and **101 "low-power"** for gyro (recommended; the DSV320X exposes 8/6 modes where the interface has one bool — choose these two and keep the bool). Never touch CTRL6 bit 4 / CTRL7 bit 7 (repurposed). **New this revision (decisions.md P1):** since high-g now always runs (A16), `xl_power_mode_set(false)` (the 111/normal branch) is no longer legal whenever high-g is enabled — CTRL1 OP_MODE_XL must stay in {000 high-performance, 001 high-accuracy ODR}. Design choice: reject a config that asks for low-g "normal" while high-g is on, or silently upgrade it and diag-log the override. Enforce at the same config-validation point A4 uses, not deep in the register-write path. | `imu_driver.c:369-373`; `imu_driver_i2c.c:295-299` | judgment (mapping choice + interlock enforcement design) | A-reg R9, R11, R32; decisions.md P1 |
| A6 | FIFO: `fifo_mode_set(STREAM_MODE)` is a rename only (code 110 unchanged). The burst/drain constants of 512 words are now *exactly* the DSV320X FIFO (512 words / 4.5 kB) — correct the three comments that attribute them to a "3 KB DSO32 FIFO" (wrong for DSO32, right by coincidence for DSV320X). No watermark is set today; if one is added, it is 8 bits (≤ 255). Tag parsing (`0x78` burst, `word[0] >> 3`, tags 0x01/0x02) is unchanged and verified for the tags already handled. **New this revision:** the tag switch must also recognize **0x1D** (high-g accel triplet) once A16 enables it — see A19, which owns that addition; this row stays scoped to the burst-constant/comment fix only. | `imu_driver.c:379`, `:48-54`; `imu_driver_i2c.c:304`, `:33-36`; `imu_task.c:31` | transcription | A-reg R20–R23; driver-impact break list |
| A7 | Dedupe before porting: `imu_driver.c` and `imu_driver_i2c.c` duplicate 14 pieces (WHO_AM_I, six enum helpers, `diag_dump_one`, init sequence, `fifo_count`, `pop_sample`, `check_overrun`/`take_pair_diag`, burst-drain loop, burst constant — `file:line` pairs in `driver-impact.md` §Mixed-fleet). The duplication was justified as "delete the I2C file in one piece" for the Adafruit stopgap; on IDL1 both buses are permanent. Move the chip logic into one module parameterised by `stmdev_ctx_t *` + per-instance state, leaving two thin bus adapters (`spi_bus_add_device` + platform fns / `i2c_master_bus_add_device` + platform fns). `imu_task.c`'s `s_imu_bus[]` dispatch (`:45-54`, `:56-74`, `:114-183`, `:305-371`) already has the right shape and stays. | `main/imu_driver.c`, `main/imu_driver_i2c.c`, new `main/imu_chip_lsm6dsv320x.c` | judgment (design; mid tier) | driver-impact §Mixed-fleet support |
| A8 | `imu_driver.h` doc contract: `accel_range_g` "4, 8, 16, or 32" → "2, 4, 8, 16"; `gyro_range_dps` "125 … 2000" → "250 … 4000"; WHO_AM_I 0x6C → 0x73; **state explicitly that off-list values are rejected, not mapped (decisions.md P2/P3 — no legacy mapping is a documented contract, not just internal behaviour).** The `idl0_imu_sample_t` struct (`:41-44`, raw `int16_t` × 6) is chip-agnostic and stays for the low-g/gyro pair; it does **not** yet have a slot for the new mandatory high-g reading — cross-reference A19, which decides where that goes. | `imu_driver.h:55-63` | transcription | driver-impact §imu_driver.h assessment; decisions.md P2, P3 |
| A9 | I2C bus config: `I2C_BUS_HZ 1000000` (`imu_driver_i2c.c:30`, comment claims DSO32 supports 1 MHz — it does not; DSV320X does) versus the **ESP32-C6 datasheet, which lists only 100/400 kbit/s** for its I2C controller. **Recommend 400 kHz, full stop** — decisions.md P2 confirms the C6 hardware ceiling is 400 kHz regardless of what the DSV320X itself supports, and separately flags the pods are now on cables (4k7 pull-ups) rather than onboard traces, so measure rise-time at bring-up before trusting any number here. Pod ODR default is now governed by D4's recomputed bus budget (this revision), not rev1's two-sensor estimate. Also set `.flags.enable_internal_pullup = false` (`:186`) — external 4k7 exist; the internal ≈ 45 kΩ only muddies the rise-time analysis. Move `s_bus` (`:177-197`) out to a bus-owner module so `tla2528` can add its device (Part D2). | `imu_driver_i2c.c:26-30`, `:177-197`, `:186` | judgment (bus speed) + transcription (pull-up flag) + judgment (small refactor) | esp32-c6-datasheet §4.2.1.3; regs-i2c; D4 (this revision); decisions.md P2 |
| A10 | Pins: IDL0 `pins.h:65-66` has SDA GPIO22 / SCL **GPIO21**; IDL1 is SDA GPIO22 / SCL **GPIO23**, and GPIO21 is now the onboard IMU's SPI **CS** (IDL0 used GPIO2 for CS — on IDL1 GPIO2 is `PWR_EN`). Reusing IDL0's `pins.h` on IDL1 would drive the load-switch enable as a chip-select. Regenerate from the IDL1 netlist per decisions.md H4's `pins_idl1.h` scheme — run 03's `pins-delta.md` is the deliverable; do not hand-edit. | `main/pins.h` → `pins_idl1.h` | transcription (from run 03) | idl1 board-facts L98–101, L106–108; driver-impact break list; decisions.md H4 |
| A11 | Address table `s_imu_i2c_addr[] = {0x6A, 0x6B}` — values unchanged (SDO/TA0 LSB, same on both chips). **Front/rear convention is now FINAL (decisions.md P7): front pod = JP1 open = 0x6A = spec IMU1; rear pod = JP1 closed = 0x6B = spec IMU2.** Re-comment accordingly; add the boot-time probe of both addresses (Part D3). Each pod already gets its own `i2c_master_dev_handle_t` on the shared bus (`imu_driver_i2c.c:249-260`), and `idl0_imu_task_start` inits each enabled I2C IMU (`imu_task.c:328-345`) — no shape change. | `imu_task.c:50-54` | transcription | driver-impact §Two addresses; A-reg R28; decisions.md P7 |
| A12 | Reset/boot: the SW_RESET poll loop (`imu_driver.c:348-354`, 10 × 2 ms) is fine — neither datasheet gives a figure. Add after reset, **SPI (onboard) instance only**: write IF_CFG (03h) bit 0 `I2C_I3C_disable = 1` (register is not reset by SW_RESET, so write it every init). Pod I2C instances: leave IF_CFG default. Gyro turn-on is 40 ms (was 35) — the Part D1 envelope covers it. | init sequence (`imu_driver.c:339-379` / shared module after A7) | transcription | A-reg R2, R3, R6, R7 |
| A13 | Comments that explain the bus split by the Adafruit level-shifter story (`imu_driver.h:20-23`, `imu_task.c:38-54`, `pins.h` "TEMP: I²C bus" block) → IDL1 rationale: one chip, onboard on SPI, pods on I2C by design. | those lines | transcription | driver-impact break list |
| A14 | Default ODR fallback 104 Hz (`imu_task.c:77`) has no DSV320X code. **No legacy mapping (decisions.md P2, FINAL — reverses rev1's Q2 "nearest ODR" recommendation).** The fallback must resolve to the same runtime-configurable default table the rest of config loading uses — **960 Hz for the onboard (SPI) IMU; pods default to 960 Hz "to be tried and measured" per decisions.md P2, with 480 Hz as the config-level fallback** — never a bare `#define` literal divorced from that table, so a drop to 480 Hz is a config push, not a reflash. Instrument `FIFO_STATUS` overrun bits from day one (decisions.md P2 requirement) so a bus that can't keep the 960 Hz default loses samples visibly, not silently. | `imu_task.c:77` | transcription (values FINAL) + judgment (source it from the config-default table, not a literal — small refactor) | A-reg R8; decisions.md P2; D4 (this revision) |
| A15 | Confirmed no-ops (do not spend time here): self-test registers are never referenced; STATUS_REG is never read (data-ready is FIFO-level polling on the 20 ms timer, `imu_task.c:264`, `imu_driver_i2c.c:324-345`); no INT routing exists; output/FIFO data addresses (22h–2Dh, 78h–7Eh) and the tag decode are identical; `idl0_imu_sample_t` and the §5.4 mask/format layer are chip-agnostic **for the low-g/gyro pair** (high-g is not a no-op — see A19). The FIFO_STATUS address move (3Ah→1Bh) and DIFF_FIFO width change live entirely inside the vendored driver. | — | — (no-op) | driver-impact §Data-ready, §Break list (no-break rows) |
| A16 | **(New — mandatory high-g, decisions.md P1/P6.)** Write `CTRL1_XL_HG` (4Eh): `XL_HG_REGOUT_EN=1`, `ODR_XL_HG[2:0]`, `FS_XL_HG[2:0]`. Add two conversion helpers `hg_odr_enum`/`hg_fs_enum` mirroring A4's pattern — real device values only, no mapping, reject off-list. Valid `ODR_XL_HG` = {480, 960, 1920, 3840, 7680} Hz (000 = power-down/disabled, 001/010 reserved); valid `FS_XL_HG` = {32, 64, 128, 256, 320} g. **Both are runtime-configurable defaults, not constants (general principle, P2), defaulting to ±64 g / 480 Hz** — the 480 Hz default matches D4's bus-budget verdict, not just the datasheet floor. Field naming for these two new config values is this checklist's proposal, not owner-specified — see Part E, Q8. | new code, `imu_driver.c` / `imu_driver_i2c.c` (after A7's shared module); no prior file:line, net-new | transcription (values FINAL per decisions.md P1 + regs-ctrl.md) | A-reg R13, R32; regs-ctrl.md high-g rows; decisions.md P1 |
| A17 | **(New — mandatory high-g.)** Set `COUNTER_BDR_REG1` (0Bh) bit 4 `XL_HG_BATCH_EN = 1` to batch the high-g stream into FIFO. **This bit is enable-only — there is no `BDR_HG` rate field.** The stream batches at whatever `ODR_XL_HG` (A16) is set to and cannot be decimated in FIFO; document this in the driver header directly next to the write, so nobody later looks for a batch-rate register that doesn't exist. | init sequence, after A16 | transcription | A-reg R31; regs-fifo.md :36, :88; decisions.md P1 |
| A18 | **(New — mandatory high-g.)** Enforce the interlock A5 introduces: reject or auto-upgrade any config that would leave `OP_MODE_XL` outside {000, 001} while high-g (A16) is enabled. This is the one place in the driver where two independent-looking config values (`accel_range_g`'s power mode and the new high-g enable) constrain each other — put the check in one function called from both config paths, not duplicated at each write site. | new code, alongside A5's enforcement point | judgment (design; where the cross-field check lives) | A-reg R32; decisions.md P1 (datasheet Table 21 constraint) |
| A19 | **(New — mandatory high-g.)** Extend the FIFO tag switch to recognize **0x1D** (high-g accelerometer triplet — X/Y/Z arrive as one tagged word via FIFO, the same shape as the existing gyro/accel tags; decisions.md P1 explicitly rejected reading output registers 34h–39h directly and abandoning FIFO timestamping for a single axis). This requires a home for the third 3-axis reading: `idl0_imu_sample_t` (`imu_driver.h:41-44`) has exactly two `int16_t[3]` fields today and no slot for a third. `driver-impact.md` flagged this as an unresolved structural gap (no record-format proposal made). **Design choice, not yet made by any prior pass:** add a third `int16_t hg_accel[3]` field to the existing struct, or emit high-g as a parallel struct / separate `CHANNEL_SAMPLE` (0x03) record path alongside the existing `IMU_SAMPLE`. Whichever is chosen must also thread through the §5.4 channel mask (14 reserved bits exist, per `driver-impact.md` §High-g path, but no convention for using them yet). | `imu_driver.h:41-44`; FIFO tag switch (`imu_driver.c:432-447`; `imu_driver_i2c.c:355-370`) | judgment (record-format design; touches spec §5.4) | driver-impact §High-g path — where it could land; regs-fifo.md tag 0x1D row; decisions.md P1 |
| A20 | **(New — mandatory high-g.)** Once A19 fixes the record format, emit registry channels **24–29** (`IMU1_HgAccelX..Z`, `IMU2_HgAccelX..Z`). Scale follows the existing pattern (`range_g / 32768`, per spec §5.2), using whichever `FS_XL_HG` value A16 has configured — sensitivities are already tabulated (±32 g → 0.976 mg/LSB … ±320 g → 10.417 mg/LSB). | new registry entries | transcription (after A19 resolves format) | regs-ctrl.md high-g sensitivity rows; decisions.md P1 |
| A21 | **(New — mandatory high-g.)** Doc/sizing note only: the high-g stream adds a third 7-byte-tagged-word producer to each pod's FIFO at its own (non-decimatable) rate. It fits the existing 512-word/4.5 kB FIFO (A6) without a depth change, but it is the dominant new term in the shared-bus load — see D4, which folds it into the recomputed pod I2C budget that sets the 960 Hz vs. 480 Hz default (A9, A14). | comment near A6's burst-constant fix | transcription (doc note) | D4 (this revision) |

Rev1's A-code was 15 rows (13 net after informational A2/A9/A14 nuance); this revision is **21 rows** — the original 15, each re-sized above, plus A16–A21.

### A-reg — Register-level mapping LSM6DSO32 → LSM6DSV320X (what every IMU code change transcribes from)

Bit positions come from `regs-bitmap.md` (DSV320X columns, line-cited) and the
vendored `lsm6dso32x_reg.h` (DSO32). Values, addresses and encodings come from
the `regs-*.md` files named in the last column. Where a `regs-*.md` bit number
disagrees with `regs-bitmap.md`, the bitmap wins for **positions**; for the
high-g **values**, `regs-ctrl.md` wins over `regs-bitmap.md`'s own decoded
`FS_XL_HG`/`ODR_XL_HG` tables — see the caution in "How to use this" above.
Three miscounts in the `regs-*.md` bit-position claims were found and
annotated at the bottom of the affected files (`regs-spi.md` IF_INC/SIM,
`regs-interrupts.md` STATUS_REG and CTRL4 DRDY_PULSED); unchanged from rev1.

| # | Item | DSO32 | DSV320X | Port note | Size | Source |
|---|---|---|---|---|---|---|
| R1 | WHO_AM_I (0Fh) | 0x6C | **0x73** | Identity check constant. | transcription | regs-identity-boot (p.66 / p.70) |
| R2 | Software reset / boot | CTRL3_C bit 0 SW_RESET, bit 7 BOOT; poll until auto-clear | CTRL3 (12h) bit 0 SW_RESET, bit 7 BOOT — same; **SW_RESET does not reset PIN_CTRL (02h) or IF_CFG (03h)**; no Tboot/Treset stated for either part | Keep the poll-until-clear loop; write IF_CFG after reset explicitly (it is not reset). | transcription | regs-identity-boot §Notes; regs-bitmap CTRL3 :2028 |
| R3 | Turn-on time | gyro 35 ms | gyro **40 ms** | Startup delay before first valid data (Part D1 envelope). | transcription | regs-identity-boot (p.28 / p.28) |
| R4 | IF_INC (auto-increment) | CTRL3_C bit 2, default 1 | CTRL3 bit 2, default 1 — same | No change. | — | regs-bitmap CTRL3 :2028 |
| R5 | BDU | CTRL3_C bit 6, default **0** | CTRL3 bit 6, default **1** | Set explicitly to 1 regardless (driver already should). | transcription | regs-ctrl BDU row |
| R6 | Interface pin config: H_LACTIVE / PP_OD / SIM | CTRL3_C bits 5 / 4 / 3 | **IF_CFG (03h)** bits 4 / 3 / 2 (SDA_PU_EN, SHUB_PU_EN, ASF_CTRL, H_LACTIVE, PP_OD, SIM, 0, I2C_I3C_disable) | Any write of these bits moves register. On IDL1 no INT pins are wired, so H_LACTIVE/PP_OD are moot; SIM stays 0 (4-wire) for the onboard SPI IMU. | transcription | regs-bitmap IF_CFG :1744; regs-spi (register moves) |
| R7 | I2C disable | CTRL4_C I2C_disable (bit 2) | **IF_CFG (03h) bit 0 I2C_I3C_disable** | Onboard SPI IMU: set 1 after reset (blocks I2C/I3C on a bus that is really SPI). Pod I2C IMUs: leave 0. I3C is on by default on DSV320X; a plain I2C master is unaffected. | transcription | regs-i2c; regs-bitmap IF_CFG :1744 |
| R8 | Accel ODR field | CTRL1_XL (10h) bits [7:4] ODR_XL; codes 0001=1.6/12.5 … 1000=833 Hz, 1001=1.66 k, 1010=3.33 k, 1011=6.66 k | CTRL1 (10h) bits **[3:0]** ODR_XL; codes 0001=1.875 Hz, 0010=7.5, 0011=15, 0100=30, 0101=60, 0110=120, 0111=240, **1000=480, 1001=960**, 1010=1.92 k, 1011=3.84 k, 1100=7.68 k | Field moved within the register AND the Hz↔code table is entirely new. Replace the ODR lookup table; **no legacy mapping — reject off-list (decisions.md P2).** | transcription | regs-ctrl ODR rows; regs-bitmap CTRL1 :1951; decisions.md P2 |
| R9 | Accel power mode | CTRL6_C bit 4 XL_HM_MODE (0 = high-perf) | CTRL1 bits **[6:4] OP_MODE_XL**, 000 = high-performance, 001 = high-accuracy ODR | Write 000 into CTRL1[6:4]; never write CTRL6 bit 4 (it is now LPF1_G_BW). **When high-g is enabled (R13/A16), OP_MODE_XL is constrained to {000, 001} — see R32.** | transcription | regs-ctrl power-mode rows |
| R10 | Gyro ODR field | CTRL2_G (11h) bits [7:4] ODR_G | CTRL2 (11h) bits **[3:0]** ODR_G; same code table as R8 (no 0001 for gyro) | Same replacement as R8; **no legacy mapping (decisions.md P2).** | transcription | regs-ctrl; regs-bitmap CTRL2 :1990; decisions.md P2 |
| R11 | Gyro power mode | CTRL7_G bit 7 G_HM_MODE | CTRL2 bits **[6:4] OP_MODE_G**, 000 = high-performance | Never write CTRL7 bit 7 (it is now INT1_DRDY_XL_HG). | transcription | regs-ctrl |
| R12 | Accel full-scale (low-g) | CTRL1_XL bits [3:2] FS_XL: 00=±4 g, 01=**±32 g**, 10=±8 g, 11=±16 g | **CTRL8 (17h) bits [1:0] FS_XL**: 00=±2 g, 01=±4 g, 10=±8 g, 11=±16 g — **no ±32 g on this path** | Register moved, encoding changed, top range gone. ±32 g exists only on the high-g accelerometer (R13), and is not a substitute for it — **no 32→16 mapping (decisions.md P1/P2, FINAL, reverses rev1's Q1 recommendation).** | transcription | regs-ctrl FS rows; regs-bitmap CTRL8 :2151; decisions.md P1, P2 |
| R13 | High-g accelerometer — **mandatory, both accelerometers run (decisions.md P1/P6, FINAL — reverses rev1's "not used unless Q1/Q6")** | — | CTRL1_XL_HG (4Eh): XL_HG_REGOUT_EN, HG_USR_OFF_ON_OUT, ODR_XL_HG[2:0], FS_XL_HG[2:0]; FS 000=±32 g, 001=±64, 010=±128, 011=±256, 100=±320; ODR 011=480 Hz, 100=960, 101=1.92 k, 110=3.84 k, 111=7.68 k; outputs **34h–39h** (not read directly — only via FIFO, see A19); FIFO tag **0x1D**; STATUS_REG bit 3 XLHGDA; self-test CTRL2_XL_HG (4Dh) | Implement per A16/A17: default ±64 g / 480 Hz, both runtime-configurable, no legacy mapping, no batch-rate field (enable-only). **Caution:** `regs-bitmap.md`'s own decoded FS_XL_HG/ODR_XL_HG tables disagree with this row (they show ±8 g…±24 g, 12.5 Hz…1667 Hz) — that decode is wrong; this row's values are cross-verified against `regs-ctrl.md`'s high-g rows and match `decisions.md` verbatim. | transcription | regs-ctrl high-g rows; regs-bitmap :3201, :3183 (address/position only); decisions.md P1 |
| R14 | Gyro full-scale | CTRL2_G bits [3:2] FS_G (00=250, 01=500, 10=1000, 11=2000) + bit 1 FS_125 | **CTRL6 (15h) bits [2:0] FS_G**: 001=±250, 010=±500, 011=±1000, 100=±2000, **101=±4000**; 000 reserved; **no ±125 dps**; CTRL6 bit 3 must be written 1 (default 0x08) | Register moved, encoding changed; preserve bit 3 = 1 on write. **No 125→250 mapping (decisions.md P3, FINAL, reverses rev1's Q3 recommendation).** | transcription | regs-ctrl gyro FS rows; regs-bitmap CTRL6 :2094; decisions.md P3 |
| R15 | Sensitivities | ±4/8/16/32 g: 0.122/0.244/0.488/0.976 mg/LSB; 125…2000 dps: 4.375…70 mdps/LSB | low-g ±2/4/8/16 g: 0.061/0.122/0.244/0.488 mg/LSB; high-g ±32…320 g: 0.976…10.417 mg/LSB; 250…4000 dps: 8.75…140 mdps/LSB | The registry scale `range/32768` (spec §5.2) already yields these; only the range→register code map changes. Now applies to A20's high-g channels too. | transcription | regs-ctrl sensitivity rows (p.12) |
| R16 | Output registers | OUT_TEMP 20h–21h, gyro 22h–27h, accel 28h–2Dh | **same addresses** | A 12-byte burst from 22h (gyro then accel) works unchanged on both parts. High-g's 34h–39h is a separate bank, read only via FIFO (A19), never burst with this one. | — | regs-ctrl §Notes; regs-spi/i2c auto-increment rows |
| R17 | STATUS_REG (1Eh) | bit 0 XLDA, bit 1 GDA, bit 2 TDA | TIMESTAMP_ENDCOUNT, 0, OIS_DRDY, GDA_EIS, XLHGDA, TDA, GDA, XLDA — **bits 0/1/2 unchanged**, bit 3 XLHGDA new | Polling code that masks 0x01/0x02 is unaffected. (`regs-interrupts.md` said otherwise — verified wrong, annotated.) Bit 3 XLHGDA is unused (this driver polls FIFO, not STATUS_REG — A15). | — | regs-bitmap STATUS_REG :2320 |
| R18 | Data-ready pulsed / masked | COUNTER_BDR_REG1 (0Bh) bit 7 dataready_pulsed; CTRL4_C bit 3 DRDY_MASK | **CTRL4 (13h)**: 0, 0, 0, INT2_on_INT1, DRDY_MASK, INT2_DRDY_TEMP, DRDY_PULSED, INT2_IN_LH → DRDY_PULSED = **bit 1**, DRDY_MASK = bit 3 | Only matters if the driver sets pulsed DRDY; no INT pins on IDL1, so it can be dropped. | transcription | regs-bitmap CTRL4 :2052 |
| R19 | INT1_CTRL (0Dh) / INT2_CTRL (0Eh) | DRDY_XL bit 0, DRDY_G bit 1, FIFO_TH bit 3, FIFO_OVR bit 4, FIFO_FULL bit 5, CNT_BDR bit 6, INT1_BOOT bit 2 | same addresses; DRDY_XL bit 0, DRDY_G bit 1, FIFO_TH/OVR/FULL/CNT_BDR same; **INT1_BOOT and DEN_DRDY_flag removed**; INT2_DRDY_TEMP moved to CTRL4 | IDL1 wires no INT pins — write 0x00 to both and delete any INT routing. | transcription | regs-bitmap INT1_CTRL :1895; regs-interrupts |
| R20 | FIFO control | FIFO_CTRL1–4 (07h–0Ah); watermark 9 bits (CTRL1[7:0] + CTRL2[0]); BDR_XL/BDR_GY codes = DSO32 ODR codes; FIFO_MODE[2:0] | same addresses; **watermark 8 bits** (CTRL2 bit 0 reserved); **BDR codes = DSV320X ODR codes (R8)**; FIFO_MODE codes same (+ 010 new); COUNTER_BDR_REG1 restructured (TRIG 2-bit, XL_HG_BATCH_EN bit 4 — R31, threshold 10 bits) | If the driver uses FIFO: cap watermark at 255 words and replace the BDR table with R8's. **Note: XL_HG_BATCH_EN (R31) is a separate enable bit, not a BDR field — it does not fit this table's pattern.** | transcription | regs-fifo; regs-bitmap COUNTER_BDR_REG1 :1857 |
| R21 | FIFO status | FIFO_STATUS1/2 at **3Ah/3Bh**; DIFF_FIFO 10 bits ([9:8] in STATUS2[1:0]) | FIFO_STATUS1/2 at **1Bh/1Ch**; DIFF_FIFO 9 bits (bit 8 in STATUS2 bit 0); WTM/OVR/FULL flags same | Address and mask change. **Read the overrun bits on every poll (decisions.md P2 instrumentation requirement) — a bus that can't keep the pod ODR loses samples silently otherwise.** | transcription | regs-fifo; regs-bitmap FIFO_STATUS1 :2262; decisions.md P2 |
| R22 | FIFO data word | TAG (78h): TAG_SENSOR[7:3], TAG_CNT[2:1], **TAG_PARITY bit 0**; data 79h–7Eh; 7-byte word | TAG (78h): TAG_SENSOR[7:3], TAG_CNT[2:1], bit 0 unused (**parity gone**); data 79h–7Eh; 7-byte word — same; tags 0x01 gyro, 0x02 accel, 0x03 temp, 0x04 timestamp **same**; **new 0x1D high-g accel (mandatory, A19)**, 0x18 high-g peak (not used — FSM peak-tracking out of scope per decisions.md P1), 0x00 empty | Drop any parity check; tag decode unchanged for the four tags already in use, **extended for 0x1D (A19)**; ignore unknown tags rather than error. | transcription | regs-fifo tag rows (p.96 / p.117); regs-bitmap :4061; decisions.md P1 |
| R23 | FIFO depth | 9 kB / 1024 words | **4.5 kB / 512 words** | Any "read up to N words" constant halves. Still sufficient headroom with the high-g stream added (A21/D4). | transcription | regs-fifo (p.50 / p.45) |
| R24 | Timestamp enable | CTRL10_C (19h) bit 5 TIMESTAMP_EN | **FUNCTIONS_ENABLE (50h)** TIMESTAMP_EN (lsm6dsv320x.md :3288); CTRL10 (19h) is now 0, EMB_FUNC_DEBUG, 0, 0, ST_G[1:0], ST_XL[1:0] | If the driver enables the timestamp counter, the write moves to 50h; TIMESTAMP0–3 (40h–43h) unchanged. | transcription | regs-bitmap CTRL10 :2227; lsm6dsv320x.md :3288 |
| R25 | Self-test bits | CTRL5_C (14h) [3:0]: ST_G[1:0] (01=pos, **11=neg**), ST_XL[1:0] (01=pos, 10=neg) | **CTRL10 (19h) [3:0]**: ST_G[1:0] (01=pos, **10=neg**), ST_XL[1:0] (01=pos, 10=neg); limits: accel 50–1700 mg (same), gyro 20–80 dps at ±250 (same) | Register moved; gyro negative code changed. Only if self-test is implemented. | transcription | regs-selftest; regs-bitmap CTRL10 :2227 |
| R26 | User offsets | X/Y/Z_OFS_USR 73h–75h; USR_OFF_W CTRL6_C; USR_OFF_ON_OUT CTRL7_G | 73h–75h same; **USR_OFF_W = CTRL9 (18h) bit 1, USR_OFF_ON_OUT = CTRL9 bit 0** | Spec §7.6 says the device never applies calibration — these should be unused; confirm and leave at 0. | transcription | regs-selftest; regs-bitmap CTRL9 :2195 |
| R27 | Temperature | OUT_TEMP 20h/21h, 256 LSB/°C, 0 = 25 °C; TODR 52 Hz | same registers and scaling; TODR 60 Hz | No change. | — | regs-selftest §Notes |
| R28 | I2C | 7-bit 0x6A (SDO/SA0 = 0) / 0x6B (= 1); 400 kHz max; auto-increment via IF_INC | 0x6A / 0x6B via SDO/TA0 — **same**; part allows 1 MHz but the **ESP32-C6 master is limited to 400 kHz**; frame format identical | Address constants unchanged; keep 400 kHz (A9, decisions.md P2). | — | regs-i2c; esp32-c6-datasheet §4.2.1.3 |
| R29 | SPI (onboard IMU) | mode 0/3, 10 MHz max, bit 0 of byte 0 = R/W, IF_INC burst | **same**; timing table now voltage-split (1.8 V / 1.2 V) — at 3.3 V VDDIO the 1.8 V column applies and is no tighter than DSO32 | Transaction code unchanged. | — | regs-spi same rows |
| R30 | CTRL registers reserved-bit hygiene | — | DSV320X marks many CTRL bits "must be 0" and CTRL6 bit 3 "must be 1"; CTRL5, CTRL7 are now IF2/I3C/high-g/LPF registers | Do read-modify-write, never whole-register constants copied from the DSO32 driver. | transcription (rule) | regs-ctrl rows; regs-bitmap |
| R31 | **(New)** High-g FIFO batching enable | — (does not exist) | COUNTER_BDR_REG1 (0Bh) bit 4 **XL_HG_BATCH_EN** — enable-only, no rate field | Set alongside A16/A17. Batches at whatever `ODR_XL_HG` is configured; cannot be decimated. | transcription | regs-fifo.md :36, :88; decisions.md P1 |
| R32 | **(New)** Low-g/high-g power-mode interlock | — | Datasheet Table 21 (cited via `decisions.md` P1): when high-g is enabled, low-g `OP_MODE_XL` must be in {000 high-performance, 001 high-accuracy ODR} — the other six OP_MODE_XL states are unavailable while high-g runs | Enforcement point is A5/A18's design choice, not a register-value question — this row exists to keep the constraint discoverable at the register level too. | judgment | decisions.md P1; regs-ctrl.md power-mode rows |

Rev1's A-reg was 30 rows (22 transcription, 8 informational). This revision
is **32 rows**: R13 flips from informational to transcription (mandatory
now), and R31–R32 are net-new — **24 transcription, 7 informational, 1
judgment (R32, the first judgment-sized row in A-reg)**.

---

## Part B — Wheel speed: ATS601LSG → XIAO GPIO0 (front) / GPIO1 (rear)

**Unchanged from rev1.** None of decisions.md's rulings (P1–P7, H1–H4, A1–A10,
P4) touch wheel-speed wiring, the ISR pattern, or the pulse-record format.
Carried forward verbatim, including sizing.

Source facts: `new-sensors.md` §ATS601LSG and §Wheel-speed signal path;
`idl1 board-facts` L98–99, L173, L162, L206–207, L311–315; spec §3.5, §5.2
rows 18/19, §5.7 wheel-pulse example, §8 `wheel_speed`.

| # | Change | Size | Spec / value |
|---|---|---|---|
| B1 | Pin constants: `IDL1_PIN_SPEED_FRONT = GPIO_NUM_0`, `IDL1_PIN_SPEED_REAR = GPIO_NUM_1` (IDL0 had GPIO4/GPIO6). Run 03's `pins-delta.md` is the authority for the header file; the values are fixed by the netlist. | transcription | idl1-facts L98–99 |
| B2 | GPIO config: input, **no internal pull** (external R14/R15 4k7 to +3V3 on the main board), interrupt on **falling edge** (ATS601 output is open-drain: idle high, pulls low on a tooth; V_OL ≤ 450 mV @ 15 mA, sink at 4k7 is 0.7 mA). GPIO0/1 are not strapping pins on the C6 (strapping = GPIO4/5/8/9/15). | transcription | new-sensors.md ATS601 table (p.2–3); C6 §2.3.5 |
| B3 | Peripheral: **GPIO ISR, timestamped per pulse** — spec §3.5 says "Interrupt-driven, timestamped per pulse (not polled)" and registry 18/19 are event-driven `pulse` channels, so PCNT (count-only) does not fit the record format. Reuse the ISR pattern in `main/digital_task.c` (`gpio_install_isr_service`, `IRAM_ATTR` handler, `xQueueSendFromISR`). PCNT stays a future option if pulse rates ever exceed a few kHz; a 60-tooth ring at 12 rev/s is ≈ 725 Hz. *Contained decision by the orchestrator; the haiku pass had suggested PCNT.* | transcription (pattern exists) | spec §3.5; digital_task.c:91–101 |
| B4 | Edge hygiene: the 4k7 + 10 nF network gives ≈ 100 µs edges (τ = 47 µs), so a falling-edge ISR sees one clean edge per tooth; add a minimum-interval guard in the ISR (drop an edge < 200 µs after the previous → rejects > 5 kHz, well above any wheel rate). IDL0's 1 ms `debounce_ms` is too coarse for a 725 Hz ring — do not reuse that value. | judgment (one number) | new-sensors.md RC check |
| B5 | Startup behaviour: ATS601 powers up ≤ 1 ms, then runs a *Startup Hysteresis → Calibration (3–9 edges) → Running* sequence and drops to *Stop* after 5 s without an edge. Firmware needs no special handling — pulses are pulses — but the first few edges after any standstill may be irregular; document it in the driver header, and do not derive "sensor missing" from a quiet line. | transcription (comment) | new-sensors.md ATS601 (p.4–5) |
| B6 | Records: one `CHANNEL_SAMPLE` (0x03) per pulse, channel 18 (front) / 19 (rear), `u32` monotonic pulse count, timestamp from `esp_timer_get_time()` in the ISR, rate 0 (event). Registry entries: name `WheelFront`/`WheelRear`, units `pulse`, scale 1.0, offset 0. Emit only when `wheel_speed.<slot>.enabled`. Speed is computed in the app (§3.5 formula) — never on device. | transcription | spec §5.2 rows 18/19, §5.7 line "Wheel pulse example", §8 `wheel_speed` |
| B7 | Placement: add a `kind: "wheel"` to `digital_task.c`'s channel model (it already owns ISR service + queue + writer submission) rather than a new task; the only differences from `marker` are the 200 µs guard (B4), the u32 counter, and channel IDs 18/19 instead of a config-assigned ID. | judgment (small) | digital_task.c:59–67 |

## Part C — Analog input: pod J2 → 6-wire → TLA2528 AIN5 (front) / AIN4 (rear), I2C 0x17

**Rewritten this revision (decisions.md P5).** Rev1's manual-mode,
firmware-triggers-each-conversion design (old C2/C4) is replaced by
**autonomous mode + auto-sequence over AIN4/AIN5 + OSR**, reading
`RECENT_CHx_LSB/MSB` on demand. Owner's scope note carried over: a generic
single-ended analog input; the transducer is not chosen. Everything below is
transducer-agnostic.

Source facts: `new-sensors.md` §TLA2528 and §Analog signal path;
**`tla2528-regmap.md` for every register address, opcode and data format**
(the register-sequence table in `new-sensors.md` is NOT to be used — its
addresses were wrong); `idl1 board-facts` L64, L140–145, L163, L174,
L197–200, L208–213, L228; spec §3.4, §5.2 rows 20/21, §8 `analog`;
`decisions.md` P5.

| # | Change | Size | Spec / value |
|---|---|---|---|
| C1 | Facts to encode as constants: TLA2528 7-bit address **0x17**; **AIN5 = front pod ANA, AIN4 = rear pod ANA** — these two are the only channels enabled in `AUTO_SEQ_CH_SEL` (bits 4/5); AIN6/AIN7 = GPIO outputs driving status LEDs D1 (red) / D2 (green, decisions.md H1) and are *not* part of the auto-sequence; AIN2/AIN3 = DNP spare header J2; AIN0/AIN1 unconnected. 12-bit conversion, AVDD = 3.3 V → 0–4095 counts over 0–3.3 V, **but firmware stores the chip's 16-bit averaged result (C6/C9), not the raw 12-bit count** (decisions.md P5). Bus: the same I2C bus as the pod IMUs (XIAO GPIO22 SDA / GPIO23 SCL, 4k7 pull-ups, 400 kHz max on the C6). | transcription | idl1-facts L64, L208–213, L228; new-sensors.md TLA table; decisions.md P5 |
| C2 | New component `components/tla2528/` (ESP-IDF `driver/i2c_master.h`, same API family `imu_driver_i2c.c` already uses). `tla2528_init(bus_handle)`: add device at 0x17; read SYSTEM_STATUS (reset 0x80); write `AUTO_SEQ_CH_SEL` = `0b00110000` (AIN4, AIN5 enabled — opcode 0x08 single-register write, register 0x12); write `SEQUENCE_CFG` = `SEQ_MODE=01b` (auto-sequence) then `SEQ_START=1b` (register 0x10) to start the sequencer free-running. No `CNVST` write, no per-read `CHANNEL_SEL` write — that was rev1's manual-mode shape and no longer applies. | transcription | tla2528-regmap.md §Register map (SEQUENCE_CFG :1019-1022, AUTO_SEQ_CH_SEL :1065), §I2C protocol (single-register write :688-691) |
| C3 | **Read path — citation gap, flag before dispatch.** `decisions.md` P5 specifies reading `RECENT_CHx_LSB/MSB` "on demand" once autonomous mode is running: a one-deep latched result per channel, no clock stretching, no conversion-start traffic. **This register's address does not appear in this run's `tla2528-regmap.md` extraction** — the register map table stops at 0x12 (`AUTO_SEQ_CH_SEL`); the file's own "Could not find" section only flags Figure 25's bit-packing as missing, not a missing register, so this gap wasn't previously surfaced. The architecture ruling itself is FINAL and not in question — only the exact register address(es) for `RECENT_CH4`/`RECENT_CH5` need pulling from `references/source/tla2528.pdf` (or a fresh docling pass over it) before this can be coded. Do not guess the address from the existing 0x0–0x12 map's pattern. | judgment (blocked on datasheet lookup — not a design choice, but not safely dispatchable as cheap transcription either) | decisions.md P5 (architecture, FINAL); tla2528-regmap.md (address gap) |
| C4 | LED dependency, carried from rev1 unchanged in substance: on IDL1 the two status LEDs are TLA2528 GPIO6/GPIO7 (decisions.md H1: D2 green = ch 7, power/BLE; D1 red = ch 6, recording), not a XIAO pin (`led_status.c` currently drives GPIO15). `PIN_CFG`/`GPIO_CFG`/`GPO_DRIVE_CFG` must set AIN6/7 as push-pull outputs while AIN4/5 stay analog — one owner module for the chip, one mutex, `led_status.c` calls into it. **Confirm this doesn't collide with C2's `AUTO_SEQ_CH_SEL` write** — AIN6/7 must never be enabled there since they're configured as GPIO, not analog input. Getting `PIN_CFG` wrong either kills the LEDs or turns the pressure inputs into GPIOs. | judgment | idl1-facts L197–200; tla2528-regmap.md §Bit fields (PIN_CFG, GPIO_CFG, GPO_DRIVE_CFG); decisions.md H1 |
| C5 | Sampling: `analog_task` reads `RECENT_CH4_LSB/MSB` then `RECENT_CH5_LSB/MSB` (C3) at `analog.sample_rate_hz` cadence (default 100 Hz, C12) — **firmware no longer triggers a conversion; it just reads the sequencer's latest latched result.** Bus cost is lower than rev1's manual-mode estimate (no `CNVST`/`CHANNEL_SEL` write per read): a block read of two channels ≈ 4–6 payload bytes ≈ well under 200 µs at 400 kHz → roughly 2% of the bus at 100 Hz, negligible next to D4's IMU budget. Emit `CHANNEL_SAMPLE` id 20 / 21, `u16` — **the chip's averaged 16-bit result, not a raw 12-bit count promoted to u16 (decisions.md P5).** Registry: `PressureFront`/`PressureRear`, `u16`, rate = `analog.sample_rate_hz`, units per config. | transcription (depends on C3's address) | spec §5.2 rows 20/21, §8 `analog.sample_rate_hz`; decisions.md P5 |
| C6 | Scale/offset: spec rows 20/21 say "from config" but §8 has no field for fixed channels — only `analog.channels[]` carries `units/scale/offset`. Until the transducer is chosen: scale **1.0**, offset **0**, units `raw` (16-bit averaged counts, not 12-bit — C1). The app converts later; no device-side math. This resolves what rev1 flagged as PROVISIONAL — Q5; decisions.md P5 confirms fixed channels 20/21 (not the generic `analog.channels[]` list) as FINAL. | transcription | decisions.md P5 (FINAL) |
| C7 | Front end is already protective: TVS (U5/U6) + 33 R series + 100 nF at the ADC pin (τ = 3.3 µs, −3 dB ≈ 48 kHz) — settles in < 20 µs, irrelevant at ≤ 500 Hz (C12's fastest valid rate). Spec §3.4's "No input protection" note does not describe IDL1 (documentation gap, logged in Part E as Q4, resolved via decisions.md P4). | transcription (doc note) | new-sensors.md settling check; idl1-facts L208–209; decisions.md P4 |
| C8 | Power-up: TLA2528 is on the switched +3V3 (TPS22919); **tPU ≤ 5 ms** after AVDD ≥ 2.35 V, **tRST ≤ 5 ms** after a software reset. Init only after the Part D delay; if init fails, log and continue — analog is optional, never a boot blocker (spec §4.5 sensor-failure policy). **Clock-stretch warning, sharpened this revision:** the device stretches SCL during every conversion and — critically — during averaging, for as long as the OSR takes to complete (C9/C10 govern this). If `OSC_SEL`/`CLK_DIV` were ever left at their power-optimized extreme (`OSC_SEL=1`, `CLK_DIV=1111b`), an OSR=128 average would stretch the bus for **786 ms** — long enough to starve both pod IMUs' FIFO drains sharing this same bus (D4) and trigger the overrun instrumentation decisions.md P2 requires. C10 pins the fast, short-stretch configuration specifically to prevent this; set the IDF transaction timeout ≥ 10 ms regardless, as insurance. | transcription | tla2528-regmap.md §Timing (:242, :269-307); spec §4.5; decisions.md P5 |
| C9 | **(New)** OSR is a **separate runtime-configurable knob from `analog.sample_rate_hz`** — it controls the chip's internal averaging depth, not how often firmware reads. Valid `OSR[2:0]` (register 0x3, `OSR_CFG`) = {0, 2, 4, 8, 16, 32, 64, 128} samples; ship with **OSR = 0** and expose the knob (decisions.md P5, explicit instruction). Field name for this new config value is this checklist's proposal, not owner-specified — see Part E, Q9. | judgment (new config field — name/spec-plumbing, not a register-value question) | tla2528-regmap.md OSR_CFG :880; decisions.md P5 |
| C10 | **(New)** Pin `OSC_SEL=0` (high-speed oscillator) and keep `CLK_DIV` low — write `OPMODE_CFG` (register 0x4) = `0x00` at init. At `OSC_SEL=0`/`CLK_DIV=0000b` a conversion cycle is ≈ 1 µs, so even OSR=128 costs ≈ 128 µs; at the opposite extreme (`OSC_SEL=1`/`CLK_DIV=1111b`) the same average would hold the bus for **786 ms** (C8). **Comment this explicitly in the driver** so nobody "optimises for power" here later — decisions.md P5's literal instruction. | transcription | tla2528-regmap.md OPMODE_CFG :899-901; decisions.md P5 |
| C11 | **(New)** Set `APPEND_STATUS[1:0] = 01b` in `DATA_CFG` (register 0x2) so the 4-bit channel ID rides with each conversion result. Cheap insurance that the sequencer is reading the channel firmware thinks it's reading — decisions.md P5's stated rationale. | transcription | tla2528-regmap.md DATA_CFG :861; decisions.md P5 |
| C12 | `analog.sample_rate_hz` valid set: **FINAL (decisions.md P5) — {10, 50, 100, 200, 500} Hz, default 100.** This resolves what rev1 logged as an open, undecided §8 field (old C8). **Reword the spec/doc meaning:** this value now means *how often firmware reads* `RECENT_CHx_LSB/MSB` (C5), not how often the ADC converts — the ADC free-runs continuously under auto-sequence regardless of this setting. Word it that way in the spec or it will be misread as a conversion rate (decisions.md P5's explicit warning). | transcription | decisions.md P5 (FINAL) |

Rev1's Part C was 8 rows, one still-open (old C8). This revision is **12
rows, all FINAL** — 9 transcription, 3 judgment (C3 blocked-on-citation, C4
carried judgment, C9 new-field naming).

## Part D — Power sequencing and bus ownership (shared by every sensor)

Source facts: `idl1 board-facts` L100, L109, L132, L159, L170, L270–292;
`regs-identity-boot.md` (Ton 40 ms); `new-sensors.md` (ATS601 ≤ 1 ms);
`regs-i2c.md`; C6 datasheet §4.2.1.3; `decisions.md` P1, P2, P5, P7.

| # | Change | Size | Spec / value |
|---|---|---|---|
| D1 | Boot order: drive `PWR_EN` (GPIO2) high → wait **≥ 50 ms** (TPS22919 rise, DSV320X boot, gyro turn-on 40 ms, ATS601 ≤ 1 ms, TLA2528 tPU) → then WHO_AM_I on SPI (onboard IMU) and at 0x6A / 0x6B (pods), then TLA2528 SYSTEM_STATUS. The 50 ms is a rounded envelope — the datasheet numbers are cited, the margin is a choice. **Unaffected by high-g or the analog architecture change** — neither adds a new boot-time dependency. | transcription (order) + judgment (the 50 ms) | idl1-facts L100, L270–292; regs-identity-boot.md "Turn-on time" |
| D2 | One I2C bus object: the bus (GPIO22/23, 400 kHz — the C6 supports only standard and fast mode; DSV320X 1 MHz / TLA2528 HS mode are unusable here; internal pull-ups off, external 4k7 exist) is shared by the TLA2528 and both pod IMUs. `imu_driver_i2c.c` currently creates the bus privately (`static s_bus`, `i2c_master_bus_config_t`); it must instead accept a bus handle from a small bus-owner module so `tla2528` can add its device. | judgment (small refactor; file:line in `driver-impact.md`) | esp32-c6-datasheet.md §4.2.1.3; imu_driver_i2c.c |
| D3 | Pod address probe: at boot try 0x6A and 0x6B; map found devices to spec IMU1 (front) / IMU2 (rear) — **FINAL per decisions.md P7: front = JP1 open = 0x6A = IMU1; rear = JP1 closed = 0x6B = IMU2**, matching rev1's recommended answer exactly. Diag-log which addresses answered with WHO_AM_I = 0x73. A pod that is absent (hardtail) is a normal condition, not an error (spec §3.2 "absent on hardtails"). | transcription | regs-i2c.md address rows; regs-identity-boot.md WHO_AM_I; decisions.md P7 |
| D4 | **Bus budget — recomputed this revision to include the mandatory high-g stream (decisions.md P1).** Rev1's D4 assumed low-g + gyro only; that arithmetic is superseded below. See "Pod I2C bus budget" section immediately following this table for the full working. **Verdict: ~80% duty cycle at 960 Hz for two pods — not comfortably viable as a shipped default; 480 Hz is the safe default until bring-up measurement (decisions.md P2's own instrumentation requirement) proves 960 Hz holds without FIFO overrun.** | judgment | This run's arithmetic (below); decisions.md P1, P2; driver-impact.md §I2C bus budget (methodology reused) |

Rev1's Part D was 4 rows with the same shape; sizing is unchanged except D3
(PROVISIONAL removed) and D4 (recomputed, verdict sharpened).

### Pod I2C bus budget (recomputed)

**Model reused from `driver-impact.md` §I2C bus budget, extended for the
mandatory high-g stream.** Assumptions stated explicitly because this number
sets a shipped default (decisions.md P2's own framing):

- I2C byte = 9 bit-times (8 data + ACK). Bit-time @ 400 kHz (Fast Mode) =
  2.5 µs.
- A burst-read transaction of `N` payload bytes costs ≈ `40 + 9×N` bit-times
  (start + addr+W + reg + repeated-start + addr+R + `N`×(data+ack) + stop,
  rounded) — the same formula `driver-impact.md` validated for the existing
  FIFO burst-drain code path (its own 960 Hz row: 39 words → 273 payload
  bytes matches independently below).
- FIFO word = **7 bytes** (1 tag + 6 data), unchanged DSO32→DSV320X
  (regs-fifo.md; A6, A22/R22).
- Poll window = **20 ms** (`IMU_POLL_MS`, `imu_task.c:28`, unchanged by any
  owner decision).
- Low-g + gyro word production at the shared pod default of 960 Hz: one XL
  word and one GY word per sample period → `words_per_poll = ceil(2 × 960 ×
  0.020) = 39` words → `N = 39 × 7 = 273` bytes.
- **High-g word production at its 480 Hz floor (decisions.md P1 — cannot be
  decimated below this, and the checklist assumes each pod ships high-g at
  the floor, matching D4's own recommended default from A16):**
  `words_per_poll = ceil(480 × 0.020) = 10` words (one triplet-tagged word
  per high-g sample, not doubled like XL+GY) → `N = 10 × 7 = 70` bytes.
- **Combined per-pod FIFO burst** (one drain transaction per pod per poll,
  matching the existing driver's burst-read shape — A6/A7): `N = 273 + 70 =
  343` bytes → `40 + 9×343 = 3127` bit-times → `3127 × 2.5 µs ≈ 7.82 ms` per
  pod.
- **Two pods, serial on one shared bus** (they cannot be read in parallel —
  the actual constraint, per `driver-impact.md`'s own note): `2 × 7.82 ms =
  15.63 ms`.
- **TLA2528 autonomous reads** (C5): decoupled from IMU ODR, driven by
  `analog.sample_rate_hz` (default 100 Hz → one read every 10 ms, so ≈ 2
  reads land inside each 20 ms poll window). Each read: a 4-byte block read
  (`RECENT_CH4` + `RECENT_CH5`, 2 bytes each) → `40 + 9×4 = 76` bit-times →
  `190 µs`. Two reads ≈ `380 µs` per 20 ms window.
- **Total bus-busy time per 20 ms poll window ≈ 15.63 ms + 0.38 ms = 16.01
  ms.**
- **% of the 400 kHz bus = 16.01 / 20 ≈ 80%** (this recurs every window, so
  it's also the steady-state duty cycle, not just a burst peak).

A stricter variant — three *separate* per-sensor transactions per pod
(gyro, low-g accel, high-g, each with its own `40`-bit-time overhead instead
of one combined burst) — gives `≈ 84%` instead of `80%`. Both land in the
same "no headroom" band; neither accounts for I2C clock stretching, bus
arbitration retries, or the ESP32-C6 driver's own per-transaction overhead
beyond the modeled bit-times, all of which only push the number higher.

**This is the same conclusion decisions.md reaches from its own ~89%
estimate** ("three sensors, 7-byte FIFO words, 480 Hz high-g floor" —
arithmetic, not measurement, per P2). The two numbers differ (80–84% here vs.
89% there) most likely because decisions.md's headline figure did not assume
the existing driver's combined-burst-per-pod read shape; that difference does
not change the operational answer, and decisions.md's ruling — 960 Hz "to be
tried and measured," 480 Hz as the fallback default, FIFO-overrun
instrumentation mandatory from day one — is FINAL and unmodified by this
recomputation.

**Verdict: 960 Hz is not safely viable as a shipped default at 400 kHz for
two pods running low-g + gyro + high-g together. Ship 480 Hz as the pod
default (A9, A14, A16) until bring-up measurement (decisions.md P2) with the
FIFO-overrun bits (R21) actively logged proves otherwise. The rate must stay
a config value (A14), so raising it later — or dropping it further — is a
config push, never a reflash.**

---

## Part E — Items that touch the canonical spec (log; resolutions and two new open items this revision)

| Question | Status | Resolution / what it still gates |
|---|---|---|
| Q1 — low-g vs high-g accel path, `accel_range_g` 32 → 16 | **RESOLVED (decisions.md P1)** | Low-g carries `IMUx_Accel*`, valid {2,4,8,16}; high-g is a separate mandatory path (channels 24–29), not a range setting on the same channel. **No 32→16 mapping** — reverses rev1's recommended answer. |
| Q2 — new ODR set, 833 → 960 Hz | **RESOLVED (decisions.md P2)** | New valid list {7.5…7680} Hz. **No legacy mapping** — reverses rev1's "nearest ODR" recommendation. Onboard default 960 Hz; pod default 960 Hz "to be tried and measured," fallback 480 Hz (D4). |
| Q3 — gyro set {250…4000} | **RESOLVED (decisions.md P3)** | Valid {250,500,1000,2000,4000}. **No 125→250 mapping** — reverses rev1's recommended answer. |
| Q4 — §3.2/§3.4/§3.5 hardware text | **RESOLVED (decisions.md P4)** | Add a "§3.x IDL1 hardware deltas" section in `idl0-app`; do not rewrite §3.2–3.5 in place. Documentation only — no firmware row gated. |
| Q5 — analog representation, `analog.sample_rate_hz` set, scale/offset home | **RESOLVED (decisions.md P5)** | Fixed channels 20/21 (not `analog.channels[]`); autonomous-mode architecture (Part C, rewritten this revision); rate set {10,50,100,200,500} Hz default 100, now meaning read cadence not conversion cadence. |
| Q6 — record high-g as channels 24+ | **RESOLVED (decisions.md P1/P6)** | Yes, now — mandatory, not deferred. Reverses rev1's "reserve IDs, implement later" recommendation. Gates A16–A21, R13, R31–R32. |
| Q7 — pod address ↔ front/rear convention | **RESOLVED (decisions.md P7)** | Front = JP1 open = 0x6A = IMU1; rear = JP1 closed = 0x6B = IMU2 — matches rev1's recommended answer exactly. Gates A11, D3. |
| **Q8 (new)** — config field name for the high-g rate/range knobs (e.g. `imu.hg_sample_rate_hz`, `imu.hg_accel_range_g`) | **OPEN — implementation detail, not owner-decided** | decisions.md P1 mandates the feature and its runtime-configurability; it does not name the config fields. This checklist's proposed names are provisional to *this document*, not to the owner — confirm before A16/C9 land in the spec. Gates A16, A20. |
| **Q9 (new)** — config field name for the TLA2528 OSR knob | **OPEN — implementation detail, not owner-decided** | Same situation as Q8: decisions.md P5 mandates the knob exist, ship at OSR=0, and be exposed — it does not name the field. Gates C9. |

No new BLE command is needed for this scope (no tare/zero — scale/offset live
in config, spec §7.6 pattern); none is proposed. This line is unchanged from
rev1.

---

## Totals

| Part | Rows | transcription | judgment | mixed | no-op |
|---|---|---|---|---|---|
| A-code | 21 | 13 | 4 | 3 | 1 |
| A-reg | 32 | 24 | 1 | — | 7 (informational) |
| B | 7 | 5 | 2 | — | — |
| C | 12 | 9 | 3 | — | — |
| D | 4 | 1 | 2 | 1 | — |

Work items (A-code + B + C + D, matching rev1's counting convention which
excludes A-reg from this line): **44 work items** — **28 transcription, 11
judgment, 4 mixed, 1 no-op.** A-reg stands separately at **32 register
mappings — 24 transcription, 7 informational, 1 judgment.**

Rev1 totalled 34 work items (21 transcription, 7 judgment, 4 mixed, 2
no-op/logged, 8 PROVISIONAL) plus 30 A-reg rows (22 transcription, 8
informational). This revision adds 10 work items (A16–A21, C9–C12 net of old
C8's merge) and 2 A-reg rows (R31–R32), and **clears every PROVISIONAL tag**
— all seven of rev1's open questions are resolved in `decisions.md`, leaving
only two new, narrower, not-yet-owner-decided field-naming details (Q8, Q9).
