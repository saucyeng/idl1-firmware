# Driver impact — IDL0 IMU path vs LSM6DSV320X (pod, I2C)

## Verdict

- **STdC replaceable?** Partially. `components/lsm6dso32x_STdC/` can be swapped for `lsm6dsv320x_STdC/` behind the *same* `stmdev_ctx_t` adapter (confirmed identical struct shape) with **zero** changes to the platform read/write/mdelay glue — but every ST enum literal used in `main/imu_driver*.c` (FS, ODR, BDR, power-mode) references a value set that changed shape, not just name, so the six `xX_fs_enum`/`xx_odr_enum`/`xx_bdr_enum` helper functions must be rewritten, not mechanically renamed.
- **New interface needed?** No new public interface is required, but `imu_driver.h`'s documented parameter contract (`accel_range_g: 4/8/16/32`, `gyro_range_dps: 125/250/500/1000/2000`) leaks DSO32-specific value sets that no longer hold; a chip-ops table (shared init/config/FIFO-parse operating only on `stmdev_ctx_t*`) is the right shape — the current split is duplicated code, not a wrong interface shape.
- **Mixed-fleet (1 SPI + 2 I2C, same chip)?** Not handled cleanly today. `imu_driver.c` and `imu_driver_i2c.c` duplicate the entire config/convert/FIFO-pairing logic byte-for-byte (documented as a deliberate deletable-in-one-piece duplication); porting to DSV320X currently means editing the same fix in two files.
- **Data-ready polling?** Already polling-only. The I2C path never touches STATUS_REG or INT lines — it polls `FIFO_STATUS` level (`lsm6dso32x_fifo_data_level_get`) on a fixed 20 ms timer. This already matches the pod's unconnected-INT1/INT2 wiring; nothing in `main/` needs to change for that reason.
- **I2C bus budget OK?** Yes at the current default ODR (104 Hz) and comfortably up to ~480 Hz; gets tight (~60% of the 20 ms poll window at 400 kHz) around 960 Hz with two IMUs on one bus — recommend the pod bus run at DSV320X's native 1 MHz Fast-Mode-Plus, which is actually correctly in-spec for the new chip (see §7 note — the old code already ran DSO32 at 1 MHz, which was arguably out of DSO32's own datasheet spec).

## Break list

| file:line | what | why it breaks | delta ref | fix class |
|---|---|---|---|---|
| `imu_driver.h:56-57` | doc: "verify WHO_AM_I (0x6C)" | DSV320X WHO_AM_I value is 0x73, not 0x6C | regs-identity-boot.md "WHO_AM_I value" | transcription |
| `imu_driver.c:46` `#define IMU_WHO_AM_I 0x6C` | identity check constant | wrong value for DSV320X (0x73) | regs-identity-boot.md "WHO_AM_I value" | transcription |
| `imu_driver.c:340-342` | `who != IMU_WHO_AM_I` check + log string "expected 0x%02X" | same constant, propagates | regs-identity-boot.md | transcription |
| `imu_driver_i2c.c:32` `#define IMU_WHO_AM_I 0x6C` | identity check constant (I2C copy) | same as above, duplicated | regs-identity-boot.md | transcription |
| `imu_driver_i2c.c:271-277` | WHO_AM_I check + log (I2C copy) | same constant, propagates | regs-identity-boot.md | transcription |
| `imu_driver.h:62` doc "`accel_range_g`: 4, 8, 16, or 32" | public param contract | DSV320X low-g accel FS is only 4/8/16 g (2-bit FS_XL field in CTRL8); ±32 g only exists on the new, separate high-g chain (CTRL1_XL_HG, different output registers) | regs-ctrl.md "Accelerometer full-scale (low-g)" + "(high-g)" rows | judgment |
| `imu_driver.c:199-208` `xl_fs_enum()` | 4/8/16/32→`LSM6DSO32X_4g..32g` switch, default falls to `32g` | DSV320X has no low-g "32g" enum member; default case is now meaningless/unsafe | regs-ctrl.md | judgment |
| `imu_driver_i2c.c:153-162` `xl_fs_enum()` (I2C copy) | same switch, duplicated | same | regs-ctrl.md | judgment |
| `imu_driver.h:63` doc "`gyro_range_dps`: 125, 250, 500, 1000, or 2000" | public param contract | DSV320X removes ±125 dps entirely (FS_G is now 3-bit: 250/500/1000/2000/4000 dps, 000 reserved) | regs-ctrl.md "Gyroscope full-scale" + "FS expansion and ±125 dps removal" note | judgment |
| `imu_driver.c:210-220` `gy_fs_enum()` | 125/250/500/1000/2000→enum switch | 125 dps case has no DSV320X target; value set also gains 4000 dps | regs-ctrl.md | judgment |
| `imu_driver_i2c.c:163-173` `gy_fs_enum()` (I2C copy) | same, duplicated | same | regs-ctrl.md | judgment |
| `imu_driver.c:139-152` `xl_odr_enum()` (12.5/26/52/104/208/417/833/1667 Hz) | accel ODR switch | DSV320X accel ODR table is a completely different set (1.875/7.5/15/30/60/120/240/480/960/1920/3840/7680 Hz) — no shared values at all | regs-ctrl.md "Accelerometer ODR encoding" | judgment |
| `imu_driver.c:154-167` `gy_odr_enum()` | gyro ODR switch | same remap, gyro side | regs-ctrl.md "Gyroscope ODR encoding" | judgment |
| `imu_driver.c:169-182` `xl_bdr_enum()` | FIFO XL batch-rate switch | FIFO_CTRL3 BDR_XL table remapped identically to the ODR table | regs-fifo.md "FIFO_CTRL3 (09h) BDR_XL[3:0] rates" | judgment |
| `imu_driver.c:184-197` `gy_bdr_enum()` | FIFO GY batch-rate switch | FIFO_CTRL3 BDR_GY table remapped identically | regs-fifo.md "FIFO_CTRL3 (09h) BDR_GY[3:0] rates" | judgment |
| `imu_driver_i2c.c:97-152` (all four ODR/BDR enum helpers, I2C copy) | duplicated switches | same four remaps, duplicated | regs-ctrl.md, regs-fifo.md | judgment |
| `imu_driver.c:369,372` `lsm6dso32x_xl_power_mode_set(..., HIGH_PERFORMANCE_MD / LOW_NORMAL_POWER_MD)` | 2-state accel power mode, written to what was CTRL6_C | DSV320X moves this to `OP_MODE_XL[2:0]` in CTRL1 (bits 6-4) with 8 states (high-perf, high-accuracy ODR, ODR-triggered, LPM1-3, normal); CTRL6 is now the gyro-FS/LPF register, no longer a power-mode register | regs-ctrl.md "Power mode — accelerometer..." + "CTRL6_C/CTRL6 re-purpose" note | judgment |
| `imu_driver.c:370,373` `lsm6dso32x_gy_power_mode_set(..., GY_HIGH_PERFORMANCE / GY_NORMAL)` | 2-state gyro power mode, written to what was CTRL7_G | DSV320X moves this to `OP_MODE_G[2:0]` in CTRL2 (bits 6-4) with 6 states incl. sleep; CTRL7 is now high-g interrupt routing + LPF1_G_EN | regs-ctrl.md "Power mode — gyroscope..." + "CTRL7_G/CTRL7 re-purpose" note | judgment |
| `imu_driver_i2c.c:295-299` (power-mode calls, I2C copy) | same 2-state calls, duplicated | same | regs-ctrl.md | judgment |
| `imu_driver.c:379` / `imu_driver_i2c.c:304` `lsm6dso32x_fifo_mode_set(ctx, LSM6DSO32X_STREAM_MODE)` | FIFO mode = Continuous (encoding value 6 / 110b) | FIFO_MODE encoding value 110 = "Continuous" is unchanged on DSV320X (only the previously-reserved value 010 gained a new meaning, "continuousWTM-to-full") | regs-fifo.md "FIFO_CTRL4 (0Ah) FIFO_MODE[2:0]" — status "changed" only for value 010 | transcription (symbol rename only) |
| `imu_driver.c:48-54` comment: "512 words = 256 pairs = the full LSM6DSO32X FIFO (3 KB, 7-byte words)" | doc comment justifying `IMU_SPI_BURST_MAX_WORDS 512` | Wrong even for DSO32 today — DSO32's actual FIFO is 1024 words / 9 kB per regs-fifo.md, not 512/3KB. By coincidence, 512 words *is* DSV320X's exact full FIFO depth (4.5 kB / 512 words), so the constant becomes correct for the new chip even though the comment's reasoning is wrong for the old one. No functional bug either way, but the comment should be corrected and re-attributed to DSV320X, not DSO32. | regs-fifo.md "FIFO depth" / "FIFO depth in words" | transcription (comment only) |
| `imu_task.c:31` comment: "full LSM6DSO32X FIFO (256 pairs)" for `IMU_DRAIN_MAX_PAIRS 256` | same coincidence as above, task-level constant | same | regs-fifo.md | transcription (comment only) |
| `imu_driver_i2c.c:33-36` comment, `IMU_BURST_MAX_WORDS 512` | same coincidence, I2C copy | same | regs-fifo.md | transcription (comment only) |
| `imu_driver_i2c.c:26-30` comment: "Fast-mode-plus (1 MHz). LSM6DSO32 supports it per datasheet" + `I2C_BUS_HZ 1000000` at line 30 | I2C bus clock, with a comment claiming DSO32 datasheet support for 1 MHz | Per regs-i2c.md, DSO32's documented max SCL is 400 kHz — the *existing* IDL0 code already runs the bus faster than DSO32's own spec allows. DSV320X genuinely adds Fast-Mode-Plus (1 MHz) support, so this setting becomes correctly in-spec for the new chip without any change — flagging so nobody "fixes" it back to 400 kHz thinking it's a new problem. | regs-i2c.md "I2C speed modes supported" / "I2C maximum SCL frequency" | transcription (comment correction only; not a functional break) |
| `pins.h:65-66` `IDL0_PIN_I2C_SDA GPIO22` / `IDL0_PIN_I2C_SCL GPIO21` | I2C pin assignment, IDL0-board-specific | Pod board is wired to XIAO GPIO22 (SDA) / GPIO23 (SCL) — SDA coincides, SCL does not (GPIO21 vs GPIO23). IDL1's onboard SPI IMU CS is *also* GPIO21 per `idl1-hardware` board-facts — an entirely different net on the new board, so IDL0's `pins.h` values must not be reused verbatim. | brief context (board-facts.md lines 53, 101-108, 220-223) | transcription (regenerate pins.h from IDL1 netlist, per its own header's stated process) |
| `imu_driver_i2c.c:186` `.flags.enable_internal_pullup = true` | I2C bus config | Pod already has external 4k7 pull-ups per the brief; internal ESP32 pull-ups (~45 kΩ) are weak enough that this is harmless, but is now redundant belt-and-braces rather than doing real work as it did for the old IDL0 bare-wire harness | brief context (pod 6-wire cable, 4k7 pull-ups) | judgment (keep-or-drop is a design call, not a correctness bug) |
| `imu_driver.h:20-23`, `imu_task.c:38-54` `s_imu_bus[]` / `s_imu_i2c_addr[]` comment framing ("IMU1/IMU2 are Adafruit 4692 breakouts...") | stale rationale text, not a functional issue | On IDL1 the bus split is no longer "onboard-vs-flaky-level-shifter", it's "onboard chip on SPI + two identical pod chips on I2C" — same code shape, different reason; comments should be updated but the `s_imu_bus[]`/dispatch mechanism itself is exactly what's needed (see Mixed-fleet section) | brief context | transcription (comment only) |
| *(no break)* self-test (ST1_XL/ST_XL, CTRL5_C/CTRL10 self-test bits) | — | Not referenced anywhere in `imu_driver*.c` / `imu_task.c` (grepped, zero hits) — self-test register relocation (CTRL5_C→CTRL10) is real per regs-selftest.md but has no call site to break | regs-selftest.md | n/a |
| *(no break)* data-ready via STATUS_REG bits | — | `main/` never reads STATUS_REG or calls `*_flag_data_ready_get` (grepped, zero hits) — the orchestrator's live correction that XLDA/GDA/TDA bit positions are unchanged (bits 0/1/2 on both chips, verified against `lsm6dsv320x.md:2320` Table 84) is moot for this driver: it doesn't touch that register at all | regs-interrupts.md (superseded by orchestrator correction + extraction Table 84) | n/a — see Could not verify |

## STdC API surface used

| lsm6dso32x symbol | used at | DSV320X equivalent | verified? | shape match? |
|---|---|---|---|---|
| `stmdev_ctx_t` (write_reg/read_reg/mdelay/handle/priv_data) | `imu_driver.c:66,331-335`; `imu_driver_i2c.c:47,262-266` | `stmdev_ctx_t` (identical fields) | **verified** (WebFetch of `lsm6dsv320x_reg.h`) | yes, identical |
| `lsm6dso32x_read_reg` | `imu_driver.c:243,247-248` (diag); `imu_driver_i2c.c:211,213-214,443` | `lsm6dsv320x_read_reg` | unverified (function prototypes not visible in the fetched header section — see Could not verify) | assumed match, per STdC calling convention `(ctx, reg, data, len)` |
| `lsm6dso32x_write_reg` | `imu_driver.c:247,250`; `imu_driver_i2c.c:213,215` | `lsm6dsv320x_write_reg` | unverified | assumed match |
| `lsm6dso32x_device_id_get` | `imu_driver.c:340`; `imu_driver_i2c.c:271` | `lsm6dsv320x_device_id_get` | unverified | assumed match; caller must update the expected-value constant (0x6C→0x73) |
| `lsm6dso32x_reset_set` / `lsm6dso32x_reset_get` | `imu_driver.c:349,352`; `imu_driver_i2c.c:280,283` | `lsm6dsv320x_reset_set` / `_reset_get` | unverified | assumed match (SW_RESET is CTRL3 bit 0 on both chips) |
| `lsm6dso32x_block_data_update_set` | `imu_driver.c:357`; `imu_driver_i2c.c:287` | `lsm6dsv320x_block_data_update_set` | unverified | assumed match (BDU same address/bit both chips, only reset default flips 0→1) |
| `lsm6dso32x_xl_full_scale_set` + `lsm6dso32x_fs_xl_t` (4g/8g/16g/32g) | `imu_driver.c:199-208,360`; `imu_driver_i2c.c:153-162,289` | `lsm6dsv320x_xl_full_scale_set` + `lsm6dsv320x_fs_xl_t` | partially verified — `LSM6DSV320X_CTRL8_t.fs_xl` field confirmed to exist (2-bit) via WebFetch; enum member names not confirmed | **shape mismatch in value space**: low-g set shrinks to 3-4 members (no 32g); high-g equivalent lives in a separate `CTRL1_XL_HG_t.fs_xl_hg` / presumably `lsm6dsv320x_xl_hg_full_scale_set` (unverified name) |
| `lsm6dso32x_gy_full_scale_set` + `lsm6dso32x_fs_g_t` (125/250/500/1000/2000dps) | `imu_driver.c:210-220,361`; `imu_driver_i2c.c:163-173,290` | `lsm6dsv320x_gy_full_scale_set` + `lsm6dsv320x_fs_g_t` | partially verified — `LSM6DSV320X_CTRL6_t.fs_g` field confirmed to exist (3-bit) via WebFetch | **shape mismatch**: 2-bit→3-bit field, 125dps removed, 4000dps added |
| `lsm6dso32x_xl_data_rate_set` + `lsm6dso32x_odr_xl_t` | `imu_driver.c:139-152,362`; `imu_driver_i2c.c:97-110,291` | `lsm6dsv320x_xl_data_rate_set` + `lsm6dsv320x_odr_xl_t` | partially verified — `LSM6DSV320X_CTRL1_t.odr_xl` field confirmed (4-bit) | function signature matches; **enum value set completely remapped** (no shared Hz values with DSO32) |
| `lsm6dso32x_gy_data_rate_set` + `lsm6dso32x_odr_g_t` | `imu_driver.c:154-167,363`; `imu_driver_i2c.c:111-124,292` | `lsm6dsv320x_gy_data_rate_set` + `lsm6dsv320x_odr_g_t` | partially verified — `LSM6DSV320X_CTRL2_t.odr_g` field confirmed (4-bit) | same as above |
| `lsm6dso32x_xl_power_mode_set` + enum (`HIGH_PERFORMANCE_MD`/`LOW_NORMAL_POWER_MD`) | `imu_driver.c:369,372`; `imu_driver_i2c.c:295,298` | `lsm6dsv320x_xl_power_mode_set` + new `OP_MODE_XL[2:0]` enum (up to 8 states) | unverified | **shape mismatch**: 2-state boolean-like enum → 8-state enum, register moves CTRL6→CTRL1 |
| `lsm6dso32x_gy_power_mode_set` + enum (`GY_HIGH_PERFORMANCE`/`GY_NORMAL`) | `imu_driver.c:370,373`; `imu_driver_i2c.c:296,299` | `lsm6dsv320x_gy_power_mode_set` + new `OP_MODE_G[2:0]` enum (up to 6 states incl. sleep) | unverified | **shape mismatch**: register moves CTRL7→CTRL2 |
| `lsm6dso32x_fifo_xl_batch_set` + `lsm6dso32x_bdr_xl_t` | `imu_driver.c:169-182,377`; `imu_driver_i2c.c:125-138,302` | `lsm6dsv320x_fifo_xl_batch_set` + `lsm6dsv320x_bdr_xl_t` | partially verified — `LSM6DSV320X_FIFO_CTRL3_t.bdr_xl` confirmed (4-bit) | value set remapped identically to the ODR table |
| `lsm6dso32x_fifo_gy_batch_set` + `lsm6dso32x_bdr_gy_t` | `imu_driver.c:184-197,378`; `imu_driver_i2c.c:139-152,303` | `lsm6dsv320x_fifo_gy_batch_set` + `lsm6dsv320x_bdr_gy_t` | partially verified — `LSM6DSV320X_FIFO_CTRL3_t.bdr_gy` confirmed | same |
| `lsm6dso32x_fifo_mode_set` + `LSM6DSO32X_STREAM_MODE` (=6) | `imu_driver.c:379`; `imu_driver_i2c.c:304` | `lsm6dsv320x_fifo_mode_set` + `LSM6DSV320X_...STREAM...` | partially verified — `LSM6DSV320X_FIFO_CTRL4_t.fifo_mode` field confirmed (3-bit); encoding value 6=Continuous confirmed unchanged via regs-fifo.md | transcription (rename only) |
| `lsm6dso32x_fifo_data_level_get` | `imu_driver.c:404`; `imu_driver_i2c.c:332,413` | `lsm6dsv320x_fifo_data_level_get` | unverified | assumed match (returns `uint16_t`; internal FIFO_STATUS1/2 addr+width difference is entirely inside the driver) |
| `lsm6dso32x_fifo_status_get` + `.fifo_ovr_ia` | `imu_driver.c:410-415`; `imu_driver_i2c.c:336-341,417-419` | `lsm6dsv320x_fifo_status_get` + `.fifo_ovr_ia` (field name unverified) | unverified | assumed match — regs-fifo.md lists `OVR_IA` as unchanged between chips |
| `lsm6dso32x_fifo_sensor_tag_get` + `lsm6dso32x_fifo_tag_t` (`GYRO_NC_TAG=1`, `XL_NC_TAG=2`) | `imu_driver.c:432,442,447`; `imu_driver_i2c.c:355,365,370` | `lsm6dsv320x_fifo_sensor_tag_get` + `lsm6dsv320x_fifo_tag_t` | tag **values verified** via regs-fifo.md ("Gyroscope NC"/"Accelerometer NC" = same tags both chips); function/enum names unverified | transcription (rename only) |
| `lsm6dso32x_fifo_out_raw_get` | `imu_driver.c:437`; `imu_driver_i2c.c:360` | `lsm6dsv320x_fifo_out_raw_get` | unverified | assumed match |
| Raw address `0x78` (FIFO_DATA_OUT_TAG, hardcoded, not an ST symbol) | `imu_driver.c:513`; `imu_driver_i2c.c:443` | same `0x78` | **verified** (regs-fifo.md "same" + extraction `lsm6dsv320x.md:1676`) | no change needed |
| Raw bit-shift `word[0] >> 3` (tag-sensor extraction, inline) | `imu_driver.c:534`; `imu_driver_i2c.c:455` | same bit layout — TAG_SENSOR[4:0] occupies bits 7:3 on both chips | **verified** (extraction `lsm6dsv320x.md:4061` table: `TAG_SENSOR_4..0 | TAG_CNT_1 | TAG_CNT_0 | -`) | no change needed |

**Plain answer:** Yes, `components/lsm6dso32x_STdC/` can be swapped for `lsm6dsv320x_STdC/` behind the same `stmdev_ctx_t` adapter — the adapter shape is confirmed identical and needs no changes. But it is not a drop-in rename at the call sites: every FS/ODR/BDR/power-mode enum literal in `main/imu_driver.c` and `main/imu_driver_i2c.c` references a value set that changed shape (fewer/more members, different registers), so the six conversion-helper functions must be rewritten against the new tables, and the WHO_AM_I constant must be updated. The pure I/O and FIFO-tag-parsing call sites (`read_reg`/`write_reg`/`fifo_out_raw_get`/tag values/the `0x78` burst address/the `>>3` bit-shift) carry over unchanged.

## imu_driver.h assessment

`imu_driver.h` leaks DSO32 specifics in two places, both in documentation-as-contract form rather than in the struct itself:

- The `idl0_imu_init_one()` doc comment (lines 55-63) enumerates accepted `accel_range_g` values (4/8/16/32) and `gyro_range_dps` values (125/250/500/1000/2000) — both value sets are wrong for DSV320X (no low-g 32g; no 125dps; DSV320X adds 4000dps). These are documented contracts that `idl0_config.json`/config validation upstream presumably also assumes.
- The WHO_AM_I value (0x6C) is documented in the same comment block (line 57).

`idl0_imu_sample_t` (lines 41-44, `int16_t accel[3]; int16_t gyro[3];`) does **not** leak chip specifics — it stores raw, unscaled LSBs, so it is chip-agnostic by construction. Nothing above the driver (`imu_task.c`, `idl0_format_imu_sample`) does any FS/ODR-dependent scaling on this struct; the mask/format layer is likewise chip-agnostic (raw axis values, spec-defined channel mask).

Everything that depends on the leaked value sets is inside `imu_driver.c`/`imu_driver_i2c.c` (the enum-conversion helpers) — `imu_task.c` only passes through whatever `uint8_t accel_range_g` / `uint16_t gyro_range_dps` values come from config; it does not interpret them.

**A chip-ops table (or per-instance variant struct) suffices** — no new public interface shape is needed. `idl0_imu_init_one()`'s signature (index, sample_rate_hz, accel_range_g, gyro_range_dps, high_performance) can stay; only the *legal value sets* documented for those parameters, and the internal conversion tables behind them, need to change. The `bool high_performance` boolean, however, is worth flagging: DSV320X's `OP_MODE_XL`/`OP_MODE_G` expose up to 8/6 states (high-accuracy ODR, ODR-triggered, multiple low-power tiers, sleep) that a single boolean cannot express — this is a judgment call for whoever designs the DSV320X chip-ops table, not something forced by the interface today.

## Mixed-fleet support

The current code does **not** run one chip type over two bus adapters cleanly — `imu_driver_i2c.c`'s own header comment admits this explicitly: *"This file deliberately duplicates the config sequence and ODR/range enum mapping from imu_driver.c rather than refactoring shared helpers into a third file... so it can be deleted in one piece."* That rationale (delete-in-one-piece for a temporary breakout board) no longer applies once both buses carry the *permanent*, same-chip pod path — the duplication becomes a double-maintenance liability rather than a deliberate isolation boundary.

Duplicated pieces, `file:line` pairs (SPI version → I2C version):

| logic | SPI (`imu_driver.c`) | I2C (`imu_driver_i2c.c`) |
|---|---|---|
| `IMU_WHO_AM_I` constant | `46` | `32` |
| `xl_odr_enum()` | `139-152` | `97-110` |
| `gy_odr_enum()` | `154-167` | `111-124` |
| `xl_bdr_enum()` | `169-182` | `125-138` |
| `gy_bdr_enum()` | `184-197` | `139-152` |
| `xl_fs_enum()` | `199-208` | `153-162` |
| `gy_fs_enum()` | `210-220` | `163-173` |
| `diag_dump_one()` (burst dump + write-readback scratch test) | `237-266` | `203-230` |
| init sequence body (WHO_AM_I check → reset-and-wait → BDU → FS/ODR → power mode → FIFO batch+mode) | `339-379` | `270-304` |
| `idl0_imu_fifo_count[...]()` (level + overrun-latch read) | `393-419` | `324-345` |
| `idl0_imu_pop_sample[...]()` (single-word tag/data read + pair builder) | `421-461` | `347-378` |
| `idl0_imu_check_overrun[...]()` / `idl0_imu_take_pair_diag[...]()` | `463-482` | `380-399` |
| burst-drain pair-parsing loop (identical algorithm; only the I/O call differs — one DMA SPI transaction vs. one `i2c`/`lsm6dso32x_read_reg` multi-byte read) | `494-561` | `401-482` |
| burst buffer sizing constant (`*_BURST_MAX_WORDS 512`) | `48-54` | `33-36` |

**Minimal shape that ports once:** everything above except the literal bus transaction (`spi_device_transmit` vs. `i2c_master_transmit`/`i2c_master_transmit_receive`) is already written against `stmdev_ctx_t*` and plain memory buffers — it has no bus-specific code in it. `stmdev_ctx_t` was designed by ST precisely to make this shareable: the write/read/mdelay function pointers *are* the bus abstraction. The port that ships once is:

1. One shared module (e.g. `imu_chip_dsv320x.c`) holding the WHO_AM_I constant, the four ODR/BDR/FS conversion tables, the init sequence, `fifo_count`, `pop_sample`, `check_overrun`/`take_pair_diag`, and the burst-drain pairing loop — all parameterized only by `stmdev_ctx_t *ctx` (plus a per-instance state struct, since these are currently file-static arrays keyed by `imu_index`).
2. Two thin bus-adapter files (what `imu_driver.c`/`imu_driver_i2c.c` shrink to) that only: add the device to the bus (`spi_bus_add_device` vs. `i2c_master_bus_add_device`), supply `platform_read`/`platform_write`/`platform_mdelay`, and call into the shared module.

`imu_task.c`'s existing `s_imu_bus[]` / `s_imu_i2c_addr[]` dispatch table (`imu_task.c:45-54`) and its per-call bus-dispatch helpers (`imu_task.c:56-74`, `114-183`, `305-371`) already anticipate exactly this "one chip type, per-instance bus" model — that layer does not need to change shape for IDL1, only its comments (currently framed around the Adafruit-breakout rationale) and its bus assignment table (IDL1 = SPI for IMU0, I2C for pods 1/2, same as today's table shape).

## Data-ready without INT

The I2C path already detects new data by **FIFO level polling on a fixed-period timer**, not by STATUS_REG data-ready bits and not by any interrupt line:

- `imu_task.c:264` — `vTaskDelay(pdMS_TO_TICKS(IMU_POLL_MS))` on a 20 ms fixed cadence (`imu_task.c:28-30`), independent of any GPIO interrupt.
- `imu_driver_i2c.c:324-345` (`idl0_imu_fifo_count_i2c`) and inline in `imu_driver_i2c.c:411-421` (`idl0_imu_drain_i2c`) — both call `lsm6dso32x_fifo_data_level_get()` (reads the FIFO's unread-word count) and `lsm6dso32x_fifo_status_get()` (reads the latched overrun flag `.fifo_ovr_ia`). Neither touches STATUS_REG's XLDA/GDA/TDA bits, and neither calls `lsm6dso32x_xl_flag_data_ready_get`/`gy_flag_data_ready_get` (grepped across `imu_driver*.c`/`imu_task.c` — zero hits).
- No code anywhere in `main/` reads INT1_CTRL/INT2_CTRL/MD1_CFG/MD2_CFG or configures an ESP32 GPIO interrupt for the IMUs.

**What DSV320X changes about this:** nothing that requires a `main/` code change. The FIFO_STATUS1/STATUS2 registers the vendored driver reads internally move address (0x3A/0x3B → 0x1B/0x1C per regs-fifo.md) and the DIFF_FIFO counter narrows (10-bit → 9-bit, consistent with the smaller 512-word FIFO) — but that's entirely inside `lsm6dso32x_fifo_data_level_get()`/`lsm6dso32x_fifo_status_get()`, which get swapped wholesale with the vendored component. The pod's unconnected INT1/INT2 is therefore not a new constraint on this driver — the existing DSO32-era I2C path was already built to not need interrupts.

One correction applied here: `regs-interrupts.md`'s claim that STATUS_REG's XLDA/GDA/TDA shifted from bits 0/1/2 to 1/2/3 on DSV320X is superseded by the orchestrator's live correction, itself independently re-verified in this pass against `idl1-hardware/references/extracted/lsm6dsv320x.md:2320` (Table 84: `TIMESTAMP_ENDCOUNT|0|OIS_DRDY|GDA_EIS|XLHGDA|TDA|GDA|XLDA`, i.e. XLDA=bit0, GDA=bit1, TDA=bit2 — unchanged from DSO32). It doesn't matter for this driver either way, since `main/` never reads that register.

## I2C bus budget

The code does **not** do a literal "per-sample read" — it already does FIFO burst reads (the file's own comments describe migrating away from per-word transactions because they "dominated the poll cycle"). The arithmetic below is for that existing burst strategy, scaled to the pod's likely ODR range, since that's the strategy that will actually run.

**Model** (approximate, ignoring I2C clock stretching/margin):
- I2C byte = 9 bit-times (8 data + ACK). A register-pointer + repeated-start read transaction of N payload bytes costs roughly `40 + 9×N` bit-times (start + addr+W + reg + repeated-start + addr+R + N×(data+ack) + stop, rounded).
- Bit-time: 2.5 µs @ 400 kHz (Fast Mode), 1.0 µs @ 1 MHz (Fast Mode Plus — DSV320X-only, see note below).
- Both accelerometer and gyroscope are batched into the FIFO at the same rate as ODR (`xl_bdr_enum`/`gy_bdr_enum` both driven by `sample_rate_hz`, `imu_driver_i2c.c:302-303`), so combined word production rate = 2 × ODR_hz words/sec (one XL word + one GY word per sample period). Over the `IMU_POLL_MS = 20 ms` window (`imu_task.c:28`): `words_per_poll = ceil(2 × ODR_hz × 0.020)`, payload bytes `N = words_per_poll × 7`.
- TLA2528 read: no byte count is specified in the inputs for this task; assumed a small single-channel read, N≈3 bytes — flagged as an **assumption**, not a verified figure.

| ODR (Hz) | words/poll (both IMUs' FIFO) | payload bytes N | one-IMU transaction time @400kHz / @1MHz | two IMUs (serial) + ADC (~3B, ~68µs@400k/27µs@1M) — total | % of the 20ms poll window @400kHz / @1MHz |
|---|---|---|---|---|---|
| 104 (current code's fallback default, `imu_task.c:77` / enum defaults) | 5 | 35 | 0.89 ms / 0.36 ms | 1.85 ms / 0.75 ms | 9.2% / 3.7% |
| 480 (nearest DSV320X-native rate to the old ~416-833 Hz range; illustrative pod-rate guess) | 20 | 140 | 3.25 ms / 1.30 ms | 6.57 ms / 2.63 ms | 32.8% / 13.2% |
| 960 (next DSV320X-native step up) | 39 | 273 | 6.24 ms / 2.50 ms | 12.55 ms / 5.03 ms | 62.8% / 25.2% |

**Reading this:** at the current firmware's default ODR (104 Hz) the existing burst-read strategy has large headroom on either bus speed. Around 480-960 Hz — a plausible range for a suspension/shock-sensing pod — the budget is still workable but starts consuming a large fraction of the poll window at 400 kHz (400 kHz is DSO32's ceiling, and the *only* speed a mixed I2C/I3C bus with older or other targets would be limited to); DSV320X's Fast-Mode-Plus (1 MHz, genuinely in-spec per regs-i2c.md, unlike DSO32) keeps the same ODR range comfortably under 25% of the poll window. Two devices sharing one physical bus **serialize** — they cannot be read in parallel — which is the actual constraint, not raw byte throughput at these rates.

**Conclusion (tag: judgment):** the current per-poll FIFO-burst strategy remains viable for the pod at any ODR a suspension IMU would plausibly use, *provided* the bus is run at DSV320X's 1 MHz Fast-Mode-Plus rather than 400 kHz — which is a design choice (not forced by any delta), and interacts with whatever bus speed the TLA2528 ADC and any future third device need. Per-word (non-burst) reads would not be viable at these rates — each of the ~20-40 words/poll would need its own ~2-transaction round trip, multiplying the overhead per word by roughly 7-8× versus one burst read; this is exactly why the DSO32-era code already moved off that approach (see the "was ~⅔ of every poll cycle" comment at `imu_driver_i2c.c` module level and mirrored in `imu_driver.c:39-43`).

Note: the existing `I2C_BUS_HZ = 1000000` at `imu_driver_i2c.c:30`, with its comment claiming "LSM6DSO32 supports it per datasheet," is already running at what regs-i2c.md says is *above* DSO32's documented 400 kHz ceiling. This isn't a new problem introduced by the port — it's a pre-existing, undetected mismatch that DSV320X's real Fast-Mode-Plus support (1 MHz) happens to retroactively legitimize.

## Two addresses on one bus

**Yes**, the current I2C path already supports two IMU instances at different 7-bit addresses on one shared bus:

- `imu_task.c:50-54` — `s_imu_i2c_addr[]` assigns `0x6A` (IMU1) and `0x6B` (IMU2) at compile time.
- `imu_driver_i2c.c:177-197` (`i2c_bus_init`) — idempotent; the bus (`s_bus`) is created once on the first `idl0_imu_init_one_i2c()` call and reused.
- `imu_driver_i2c.c:249-260` — each call to `idl0_imu_init_one_i2c(imu_index, i2c_addr, ...)` calls `i2c_master_bus_add_device()` with that instance's own `i2c_addr`, producing a distinct `i2c_master_dev_handle_t` per IMU (`s_i2c[imu_index].dev`) while sharing the one `s_bus` handle.
- `imu_task.c:328-345` (`idl0_imu_task_start`) — calls `idl0_imu_init_one_i2c()` once per enabled I2C-bus IMU index, each with its own address from `s_imu_i2c_addr[]`.

This maps directly onto the pod design: front/rear pods at two different 7-bit addresses (0x6A / 0x6B — same base address family `110101x`, confirmed unchanged between DSO32 and DSV320X per regs-i2c.md, LSB set by SDO/TA0), sharing one bus with a TLA2528 at a third address (0x17). No code-shape change is needed here — only the two address *values* and their pairing to front/rear (rather than IMU1/IMU2-Adafruit) are a config-level concern, and the TLA2528 device itself needs its own `i2c_master_bus_add_device()` call on the same `s_bus` (not present today — that device doesn't exist in `imu_driver_i2c.c`, which only knows about IMUs).

## High-g path — where it could land (report only)

- **Sample struct:** `idl0_imu_sample_t` (`imu_driver.h:41-44`) has exactly two 3-axis `int16_t` fields (`accel[3]`, `gyro[3]`) and no third field. There is no existing slot for a high-g accelerometer reading; adding one would require a new field (e.g. a third `int16_t[3]`) or a parallel struct.
- **Channel mask:** per `idl1-program/briefs/spec-5.4.md`, the 32-bit IMU channel mask allocates 6 bits per IMU × 3 IMUs (bits 0-17) and leaves bits 18-31 (14 bits) reserved. Bit-count-wise there is room to represent a fourth 3-axis channel for one IMU (needs 3 bits minimum for a coarse per-IMU high-g-present flag, or 9 bits for three per-IMU high-g enables) within the 14 reserved bits, but no such convention exists in the current spec, and the FIFO/format code only recognizes the two existing tags (gyro-NC, accel-NC) — high-g's new FIFO tag (0x1D) and output registers (34h-39h, distinct from the standard 28h-2Dh accel outputs) are not read anywhere in `main/` today.
- No record-format proposal is made here, per the brief's instruction — this is reported as a structural gap for the orchestrator to resolve.

## Could not verify

- **DSV320X function/enum names**: the one permitted WebFetch of `lsm6dsv320x_reg.h` returned register-struct definitions and the `stmdev_ctx_t` shape, but not the function prototype section or the enum value-name lists (the summarizing pass over the fetched content reported the file as register-struct-only; the actual upstream `lsm6dsv320x_reg.h` almost certainly also contains the function declarations, following the same single-header pattern as the vendored `lsm6dso32x_reg.h` in this repo, but that section wasn't returned). All `lsm6dsv320x_*` function and enum-member names in the STdC API table above are therefore reasoned from ST's consistent per-family naming convention (confirmed at least for struct names: `LSM6DSV320X_CTRL1_t`, `LSM6DSV320X_FIFO_CTRL4_t`, `LSM6DSV320X_FIFO_DATA_OUT_TAG_t`, `LSM6DSV320X_CTRL1_XL_HG_t` all mirror the `LSM6DSO32X_*` pattern), marked **unverified** in the table. Only the brief's one-WebFetch allowance was available; not retried.
- **Orchestrator's two live corrections, checked and applied:**
  1. STATUS_REG (1Eh) XLDA/GDA/TDA bit positions: independently re-verified against `idl1-hardware/references/extracted/lsm6dsv320x.md:2320` (Table 84) — confirmed XLDA=bit0, GDA=bit1, TDA=bit2 on DSV320X, unchanged from DSO32; `regs-interrupts.md`'s "changed" rows for these three bits are wrong. No effect on this driver either way — `main/` never reads STATUS_REG (see Data-ready section).
  2. IF_CFG (03h) SIM/I2C_I3C_disable bit positions: independently re-verified against `idl1-hardware/references/extracted/lsm6dsv320x.md:1735-1750` (Table 31) — confirmed SIM=bit2, I2C_I3C_disable=bit0; `regs-spi.md`'s "IF_CFG (03h), bit [1]" for SIM is wrong. No effect on this driver — `main/` never references SIM or I2C_I3C_disable (grepped, zero hits); the SPI driver's default 4-wire mode and the I2C driver's plain-I2C operation don't depend on this register.
  3. Both corrections reinforce the orchestrator's general note to distrust regs-*.md bit-position claims specifically; this pass additionally independently verified the FIFO_DATA_OUT_TAG bit layout (`lsm6dsv320x.md:4061`, TAG_SENSOR[4:0] = bits 7:3, matching the firmware's `word[0]>>3` extraction and contradicting `regs-fifo.md`'s "bit 7 TAG_PARITY" row, which appears to have the parity bit's position wrong too — parity is bit 0, not bit 7, on both chips). No break resulted, since the firmware's extraction is address/bit-correct regardless.
- **BOOT/SW_RESET completion timing**: neither datasheet (per `regs-identity-boot.md`'s own "Could not cite" section) specifies how long BOOT or SW_RESET take to complete beyond "auto-clears". `imu_driver.c:348-354`'s poll loop (10 × 2 ms = 20 ms max wait) is unverified against any spec figure for either chip — not a DSV320X-specific regression, just an unresolved gap in both datasheets.
