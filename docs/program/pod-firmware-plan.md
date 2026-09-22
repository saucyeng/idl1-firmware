# IDL1 Pod Sensor Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port `idl0-firmware`'s three-IMU LSM6DSO32 path to the LSM6DSV320X (onboard + two pods), add the mandatory pod high-g accelerometer stream, add pod analog input via the TLA2528, and add wheel-speed pulse capture — landing in a new `idl1-firmware` repo, not `idl0-firmware`.

**Architecture:** One shared "chip-ops" module drives the LSM6DSV320X over either SPI (onboard) or I2C (pods) via ST's `stmdev_ctx_t` bus abstraction, replacing today's file-duplicated SPI/I2C drivers. Pod I2C (IMUs + TLA2528) shares one bus-owner module. A new `tla2528` component drives the pod analog ADC in genuine TLA2528 auto-sequence mode (not the ADS7138-derived autonomous-mode design earlier drafts assumed). Wheel speed reuses the existing ISR-driven digital-channel pattern. Every sensor rate/range/OSR value is a runtime config field, never a `#define`.

**Tech Stack:** ESP-IDF (XIAO ESP32-C6), C99, vendored ST `stmdev_ctx_t`-style register drivers, host-compiled unit tests (`cc -std=c99`) for pure format/logic code.

**Spec:** `idl0-app/docs/IDL0_SPEC.md` (canonical, not edited from firmware). This plan's ground truth for what to build is `C:/Users/isaac/Documents/Saucy/saucyeng/idl1-program/decisions.md` (owner rulings, **authoritative over every other source below where they conflict**) and `C:/Users/isaac/Documents/Saucy/saucyeng/idl1-program/findings/2026-08-31-pod-delta/PORT-CHECKLIST-rev2.md` (work breakdown). Register facts are cross-checked against `.../findings/2026-08-31-pod-delta/regs-ctrl.md`, `regs-fifo.md`, `regs-identity-boot.md`, `driver-impact.md`, and `tla2528-regmap.md` in that same directory.

## Precondition — read before starting Task 1

**`idl1-firmware` does not exist yet.** This plan executes at fork time: `idl1-firmware` is created as a fresh clone of `idl0-firmware` **after** `idl0-firmware` has already absorbed decisions.md's IDL0-audit rulings (A1–A10, P4 — config mutex refusal, first-fix rename, NMEA integer parsing, zero-filled IMU sentinel, SD-full stop-and-report, header `imu_count` removal + schema bump, BLE notify-ordering doc, IDL1 hardware-deltas spec section). Those rulings are **out of scope for this plan** — they are upstream `idl0-firmware` work, assumed already merged into the commit this fork starts from. Do not implement them here; if `idl1-firmware` is forked from a commit that predates them, stop and get that fork point fixed first, rather than re-deriving them inside pod-firmware work.

This plan never edits `idl0-firmware`, `idl0-app`, `idl-rs`, or `idl1-hardware`. All file paths below are relative to the new `idl1-firmware` repo root unless stated otherwise.

## Open questions — NONE. Both were resolved in orchestrator review (2026-09-02) before execution.

1. **RESOLVED — SPI/SD GPIOs.** The plan's inputs were missing two artifacts that exist: `idl1-program/findings/2026-08-31-hw-delta/pins-delta.md` (netlist-derived, in the hw-delta dir, not the pod-delta dir this plan searched) and `idl1-program/findings/2026-09-02-pins-idl1/pins_idl1-spec.md` (the finalized full header content — use it as Task 13's source of truth). Both agree: **SPI SCK/MOSI/MISO = GPIO19/18/20, unchanged from IDL0; `IDL1_PIN_CS_SD = GPIO_NUM_7`** (moved from 23 precisely because GPIO23 is now SCL — the collision this plan flagged is real and already resolved by the netlist). Task 13 is amended below; no `#error` placeholders needed.
2. **RESOLVED — Figure 25 byte layout**, recovered via `pdftotext -layout` from `references/source/tla2528.pdf` (p.17, SBAS961A):
   - Frame A (OSR=0): byte0 = `D11..D4`, byte1 = `D3..D0 0000` — the 12-bit result **left-justified** in a big-endian 16-bit word.
   - Frame B (OSR>0): byte0 = `D15..D8`, byte1 = `D7..D0` — plain 16-bit big-endian.
   - Frame C (OSR=0 + APPEND_STATUS): as A, but the low nibble of byte1 carries the 4-bit channel ID instead of zeros.
   - Frame D (OSR>0 + APPEND_STATUS): **three bytes** — `D15..D8`, `D7..D0`, then `channel-ID<<4 | 0000`; the read clock-stretches for conversion time.

   **Design consequence (use in Task 23):** frame A is frame B with the low nibble zeroed, so `value16 = (byte0 << 8) | byte1` is the correct stored value at every OSR — oversampling is transparent to storage, exactly matching decisions.md P5's 16-bit ruling. With APPEND_STATUS on, reads are 2 bytes at OSR=0 and 3 bytes at OSR>0, channel ID in the final nibble (mask it out of the stored value in frame C). The channel-ID field IS trustworthy for routing.

## Resolved ambiguities (not left open — reasoning given so a reviewer can check the call)

- **High-g is scoped to the two pod IMUs only, not the onboard IMU0.** decisions.md's P1/P6/P2/P3/P7/H2/H3 rulings all sit under its `## Pod IMU — LSM6DSV320X` section header, and the registry channel allocation is exactly 24–29 = 2 IMUs × 3 axes. The onboard IMU0 (also physically an LSM6DSV320X, per rev2's wiring note) gets the same DSO32→DSV320X low-g/gyro register port as the pods (Tasks 5–7, 11–17) but its `CTRL1_XL_HG.XL_HG_REGOUT_EN` stays 0 — no high-g enable, no channel allocation beyond 29. If onboard high-g is wanted later, that needs new channel IDs and a fresh owner ruling; this plan does not invent them.
- **High-g record format (rev2 A19's unresolved "judgment" row).** decisions.md names six independently-scaled channels (`IMU1_HgAccelX..Z`, `IMU2_HgAccelX..Z`), not one 3-axis IMU_SAMPLE-shaped record per pod — that argues directly for the existing generic `CHANNEL_SAMPLE` (0x03) primitive (already used for wheel pulses and markers), not for extending `idl0_imu_sample_t` with a third field. Extending the struct would also force joining high-g into the same gyro/accel pair-builder despite running at a rate the owner has explicitly said can differ from low-g/gyro (decisions.md P2) — a real correctness hazard. Task 9 resolves this: high-g gets its own FIFO-tag handler emitting three back-to-back `CHANNEL_SAMPLE` records per triplet, entirely independent of the gyro/accel pairing state machine.
- **TLA2528 read architecture supersedes rev2's own Part C.** rev2's Part C (C1–C12) was written against an intermediate, since-corrected draft of decisions.md's P5 ruling that still assumed an autonomous mode with `RECENT_CHx_LSB/MSB` latch registers. decisions.md's ruling as it stands today explicitly says none of that exists on the real part, and independently confirms it: `tla2528-regmap.md`'s register table (extracted from the real datasheet) stops at 0x12 and contains no `RECENT_CHx`. **This plan does not transcribe rev2's C1/C3/C5/C12 as written.** Task 21–24 rebuild the analog path against the corrected architecture: genuine TLA2528 auto-sequence mode, host-initiated reads via a bare I2C read frame (no register pointer), each read both triggers a conversion and returns the previous one's result, and the sequencer deterministically alternates AIN4/AIN5. rev2's C2, C4 (mechanism), C6, C7, C8, C9, C10, C11 are unaffected by the correction and are transcribed as written.
- **TLA2528 I2C address is 0x10, not 0x17.** rev2's Part C and Part D cite 0x17 throughout (the `R1=0Ω/R2=DNP` row). decisions.md's H2 ruling — resolved from the exported netlist, not from rev2's board-facts reading — is that `ADDR` ties to GND through net-tie NT1, making the board's row `R1=DNP, R2=0Ω`, which is **not in TI's address table** (unspecified, not prohibited). The owner's resolution: expect **0x10** (the `R1=DNP, R2=DNP` row — what the board becomes once NT1 is cut), as a single named constant, probed once at boot with a loud failure on mismatch, never scanned. Every task below uses `IDL1_TLA2528_ADDR_EXPECTED` (0x10), not 0x17.

## Global Constraints

- **Landed-code claims are not authoritative.** Any statement in this plan, a
  brief, or a report about code that already exists is a hypothesis: grep and
  confirm at the cited file:line before building on it, and write rulings from
  the code, never from an agent's report of the code (CONTRACT.md, adopted from
  the idl1-app session's 8-for-8 catch rate on exactly this failure).

- Every sensor rate, range, and oversampling value is a **runtime-configurable default**, never a compile-time constant divorced from config. This applies to `imu.sample_rate_hz`, `imu.accel_range_g`, `imu.gyro_range_dps`, `imu.hg_range_g`, `imu.hg_sample_rate_hz`, and `analog.osr` uniformly (decisions.md P2, restated per-field throughout).
- **No legacy value mapping anywhere.** Off-list config values are rejected at validation time (before reaching any register-write helper), logged, and the prior valid setting is kept. Never silently remap or clamp to "nearest" (decisions.md P2/P3, explicit principle).
- Valid `imu.sample_rate_hz` = {7.5, 15, 30, 60, 120, 240, 480, 960, 1920, 3840, 7680} Hz. Onboard (SPI) default **960 Hz**. Pod (shared I2C) default **480 Hz** (decisions.md P2 + rev2 D4's recomputed ~80–84% duty-cycle verdict at 960 Hz for two pods running low-g+gyro+high-g — not safely viable as a shipped default; 960 Hz stays available as a config value to try at bring-up, never removed).
- Valid `imu.accel_range_g` (low-g) = {2, 4, 8, 16}. Valid `imu.gyro_range_dps` = {250, 500, 1000, 2000, 4000}.
- Valid `imu.hg_range_g` = {32, 64, 128, 256, 320}, default **64**. Valid `imu.hg_sample_rate_hz` = {480, 960, 1920, 3840, 7680} (the `ODR_XL_HG` ladder — floor 480, the hardware floor not a policy), default **480**. Field names per decisions.md Q8, resolved FINAL — not open.
- Valid `analog.osr` = {0, 2, 4, 8, 16, 32, 64, 128}, default **0**. Field name per decisions.md Q9, resolved FINAL — not open.
- Valid `analog.sample_rate_hz` = {10, 50, 100, 200, 500} Hz, default 100. Means "how often firmware reads **and converts**" (decisions.md P5, corrected wording — not "how often firmware reads a free-running latch").
- WHO_AM_I for every LSM6DSV320X instance (onboard + both pods) = **0x73** (was 0x6C on the DSO32).
- Registry channels: low-g/gyro stay `IMUx_Accel*`/`IMUx_Gyro*` on channels 0–17 (unchanged shape). High-g: channels **24–29** (`IMU1_HgAccelX..Z` = 24/25/26, `IMU2_HgAccelX..Z` = 27/28/29), FIFO tag **0x1D**. Wheel: channel **18** (front), **19** (rear). Analog: channel **20** (front, AIN5), **21** (rear, AIN4).
- Pod I2C addresses: **0x6A** = front = spec IMU1 (JP1 open). **0x6B** = rear = spec IMU2 (JP1 closed). TLA2528: **0x10** expected, single named constant, no scanning.
- I2C bus speed: **400 kHz, FINAL.** The ESP32-C6's I2C controller supports only 100/400 kbit/s per its datasheet §4.2.1.3 — the DSV320X's own 1 MHz Fast-Mode-Plus support is moot on this host. Internal pull-ups off (`enable_internal_pullup = false`); external 4k7 exist on the pod cables.
- `PWR_EN` = GPIO2 gates switched +3V3 for every pod/analog component. Drive it high once at the start of boot, before any bus or sensor init, and never revert it to an input.

---

## File structure

New/modified files in `idl1-firmware` (paths relative to repo root):

- `main/Kconfig.projbuild` — new. Board-select choice.
- `main/pins.h` — rewritten to a thin `#if` wrapper (was the full pin list).
- `main/pins_idl0.h` — new. IDL0's current `pins.h` content, moved verbatim.
- `main/pins_idl1.h` — new. IDL1 pin/channel constants, built up across Tasks 1, 13, 17, 18, 21, 22.
- `components/lsm6dsv320x_STdC/` — new. Vendored ST driver, same layout as `components/lsm6dso32x_STdC/`.
- `components/lsm6dso32x_STdC/` — kept, unchanged, selected only for `CONFIG_IDL_BOARD_IDL0` builds.
- `main/imu_chip_dsv320x.c` / `.h` — new. Shared chip-ops module (init, convert-tables, FIFO drain/pair/triplet parse), parameterized by `stmdev_ctx_t*`.
- `main/imu_driver.c` — shrinks to the SPI bus adapter (device add, `platform_read/write/mdelay`, calls into `imu_chip_dsv320x`).
- `main/imu_driver_i2c.c` — shrinks to the I2C bus adapter, same shape.
- `main/imu_driver.h` — updated doc contract (value sets, WHO_AM_I, high-g doc).
- `main/i2c_bus.c` / `.h` — new. Shared pod I2C bus owner (`idl1_i2c_bus_get()`).
- `main/imu_task.c` — bus dispatch table, address table, high-g wiring, FIFO-overrun diag.
- `main/idl0_format.c` / `.h` — new `idl0_format_hg_accel_sample()` helper (thin wrapper over existing `idl0_format_channel_sample()`).
- `main/idl0_config.h` / `.c` — new fields: `hg_range_g[3]`, `hg_sample_rate_hz[3]`, `analog_osr`.
- `main/digital_task.c` / `.h` — extended with a `kind = wheel` channel variant.
- `components/tla2528/` — new component: `tla2528.c` / `tla2528.h`, `CMakeLists.txt`.
- `main/analog_task.c` / `.h` — new. Polls the TLA2528, alternates AIN4/AIN5, emits channels 20/21.
- `main/led_status.c` — modified to route the two board LEDs through TLA2528 GPO instead of a XIAO pin.

---

### Task 1: Board-select scaffold (decisions.md H4)

**Files:**
- Create: `main/Kconfig.projbuild`
- Create: `main/pins_idl0.h` (moved from `main/pins.h`)
- Create: `main/pins_idl1.h` (skeleton only — real defines land in later tasks)
- Modify: `main/pins.h` (rewritten)

**Interfaces:**
- Produces: `CONFIG_IDL_BOARD_IDL0` / `CONFIG_IDL_BOARD_IDL1` Kconfig symbols, consumed by every later task's `#include "pins.h"` and by Task 2's `main/CMakeLists.txt` component selection.

**Tier: cheap — mechanical, fully specified by decisions.md H4.**

- [ ] Step 1: Create `main/Kconfig.projbuild`:
```
menu "IDL board selection"

choice IDL_BOARD
    prompt "IDL board variant"
    default IDL_BOARD_IDL1
    help
        Selects which board's pin map and vendored IMU driver to build.

    config IDL_BOARD_IDL0
        bool "IDL0 (three-IMU SPI/I2C prototype)"

    config IDL_BOARD_IDL1
        bool "IDL1 (pod + analog + wheel board)"
endchoice

endmenu
```
- [ ] Step 2: Copy the current `main/pins.h` content verbatim into a new `main/pins_idl0.h`, keeping every `IDL0_PIN_*` macro name unchanged (H4 only mandates the `IDL1_PIN_*` prefix split for the new board).
- [ ] Step 3: Replace `main/pins.h` with:
```c
#pragma once

#if CONFIG_IDL_BOARD_IDL1
#include "pins_idl1.h"
#elif CONFIG_IDL_BOARD_IDL0
#include "pins_idl0.h"
#else
#error "No IDL board selected — set CONFIG_IDL_BOARD_IDL0 or CONFIG_IDL_BOARD_IDL1 in menuconfig"
#endif
```
- [ ] Step 4: Create `main/pins_idl1.h` skeleton:
```c
/* IDL1 board pin assignments.
 *
 * IDL1_PIN_* for real GPIOs, typed gpio_num_t. IDL1_TLA_CH_* for TLA2528
 * channels, plain ints — kept in a separate namespace on purpose: TLA2528
 * channel 7 and ESP32 GPIO7 (SD_CS on IDL0) are a live collision risk, and
 * a shared prefix would let a GPIO_NUM_7 LED macro silently chip-select
 * the SD card. Drivers include "pins.h" only, never this file directly.
 */

#pragma once

#include "driver/gpio.h"

/* Populated by later tasks: PWR_EN (Task 17), SPI/I2C bus pins and the
 * onboard IMU CS (Task 13), wheel-speed pins (Task 18), TLA2528 channel
 * constants (Tasks 21-22). */
```
- [ ] Step 5: Build (or at minimum `idf.py reconfigure`) to confirm the Kconfig choice appears in `idf.py menuconfig` and `CONFIG_IDL_BOARD_IDL1` is set by default.
- [ ] Step 6: Commit.

---

### Task 2: Vendor `lsm6dsv320x_STdC` component (rev2 A2)

**Files:**
- Create: `components/lsm6dsv320x_STdC/CMakeLists.txt`, `idf_component.yml`, `LICENSE`, `README.md`, `lsm6dsv320x_reg.c`, `lsm6dsv320x_reg.h`
- Modify: `main/CMakeLists.txt`

**Interfaces:**
- Produces: `stmdev_ctx_t` (identical shape to the DSO32 component's — write_reg/read_reg/mdelay/handle/priv_data — verified in `driver-impact.md`'s STdC API surface table), plus every `lsm6dsv320x_*` register-access symbol Task 3 will confirm names for.

**Tier: cheap — the "keep both driver components" design question rev2 flagged as judgment is already resolved by Task 1's Kconfig board-select; this task is vendoring plus one conditional CMake block.**

- [ ] Step 1: Pull `lsm6dsv320x_reg.c`/`lsm6dsv320x_reg.h` from `github.com/STMicroelectronics/lsm6dsv320x-pid` (the upstream repo `driver-impact.md` cites), pinned to its latest tagged release at vendoring time.
- [ ] Step 2: Copy `components/lsm6dso32x_STdC/CMakeLists.txt` and `idf_component.yml` as templates into `components/lsm6dsv320x_STdC/`, updating the component name and the pulled-tag reference. Copy `LICENSE` from upstream. Write `README.md` in the same format as the DSO32 component's, recording the exact tag pulled in Step 1 (this is the project's only record of which upstream commit is in the tree).
- [ ] Step 3: In `main/CMakeLists.txt`, before `idf_component_register(...)`, add:
```cmake
if(CONFIG_IDL_BOARD_IDL1)
    list(APPEND IDL_IMU_REQUIRES lsm6dsv320x_STdC)
else()
    list(APPEND IDL_IMU_REQUIRES lsm6dso32x_STdC)
endif()
```
  and change the existing `REQUIRES` line's vendored-driver entry from the literal `lsm6dso32x_STdC` component name to `${IDL_IMU_REQUIRES}`.
- [ ] Step 4: Build both configs (`idf.py -DIDL_BOARD=idl0 build` equivalent via menuconfig, then IDL1) and confirm each links against only its own vendored driver — grep the build log for `lsm6dso32x_STdC` appearing exactly once (IDL0 build) or zero times (IDL1 build), and vice versa for `lsm6dsv320x_STdC`.
- [ ] Step 5: Commit.

---

### Task 3: Verify/apply ST symbol renames (rev2 A3)

**Files:**
- Read-only reference: `components/lsm6dsv320x_STdC/lsm6dsv320x_reg.h` (from Task 2)
- Reference: `driver-impact.md` §"STdC API surface used" (18-row table)

**Interfaces:**
- Produces: a confirmed symbol-name list Task 4 codes directly against.

**Tier: cheap — mechanical grep-and-confirm against a table `driver-impact.md` already built; every name is "reasoned by ST convention, unverified" per that table, so this task is the verification step, not new research.**

- [ ] Step 1: For each `lsm6dso32x_*` symbol in `driver-impact.md`'s STdC API surface table (`stmdev_ctx_t`, `_read_reg`, `_write_reg`, `_device_id_get`, `_reset_set`/`_reset_get`, `_block_data_update_set`, `_xl_full_scale_set`+`_fs_xl_t`, `_gy_full_scale_set`+`_fs_g_t`, `_xl_data_rate_set`+`_odr_xl_t`, `_gy_data_rate_set`+`_odr_g_t`, `_xl_power_mode_set`, `_gy_power_mode_set`, `_fifo_xl_batch_set`+`_bdr_xl_t`, `_fifo_gy_batch_set`+`_bdr_gy_t`, `_fifo_mode_set`, `_fifo_data_level_get`, `_fifo_status_get`, `_fifo_sensor_tag_get`+`_fifo_tag_t`, `_fifo_out_raw_get`), run:
```bash
grep -n "lsm6dsv320x_${SUFFIX}" components/lsm6dsv320x_STdC/lsm6dsv320x_reg.h
```
  replacing `${SUFFIX}` with the DSO32 symbol's suffix after `lsm6dso32x_`.
- [ ] Step 2: For any symbol that does not match verbatim, record the actual declared name found in the header. Do not guess — if a symbol truly isn't found, flag it in a code comment at its first call site in Task 4/5/6 rather than silently substituting a similar-looking name.
- [ ] Step 3: Also confirm the two high-g-specific symbols this port needs that have no DSO32 analogue: search for `lsm6dsv320x_xl_hg_full_scale_set` / `lsm6dsv320x_xl_hg_data_rate_set` (or their actual names) and `LSM6DSV320X_CTRL1_XL_HG_t`. Record what's found for Task 8.
- [ ] Step 4: Commit the recorded name list as a comment block at the top of `main/imu_chip_dsv320x.h` (created in Task 4) so Task 4's implementer has it without re-deriving.

---

### Task 4: Chip-ops dedupe module (rev2 A7)

**Files:**
- Create: `main/imu_chip_dsv320x.c`, `main/imu_chip_dsv320x.h`
- Modify: `main/imu_driver.c` (shrink to SPI adapter)
- Modify: `main/imu_driver_i2c.c` (shrink to I2C adapter)

**Interfaces:**
- Consumes: `stmdev_ctx_t` and symbol names from Tasks 2–3.
- Produces (in `imu_chip_dsv320x.h`):
```c
typedef struct {
    idl0_imu_state_t per_state;
    bool     overrun_flag;
    int16_t  pending_gyro[3];
    bool     have_gyro;
    bool     gyro_fresh;
    uint32_t reuse_count;
    uint32_t unknown_count;
} imu_chip_instance_t;

bool imu_chip_init(stmdev_ctx_t *ctx, imu_chip_instance_t *inst,
                    uint16_t sample_rate_hz, uint8_t accel_range_g,
                    uint16_t gyro_range_dps, bool high_performance,
                    bool hg_enabled, uint16_t hg_sample_rate_hz,
                    uint16_t hg_range_g);
uint16_t imu_chip_fifo_count(stmdev_ctx_t *ctx, imu_chip_instance_t *inst);
size_t   imu_chip_drain(stmdev_ctx_t *ctx, imu_chip_instance_t *inst,
                         idl0_imu_sample_t *out, size_t max_pairs,
                         int16_t out_hg[][3], size_t max_hg, size_t *n_hg);
bool     imu_chip_check_overrun(imu_chip_instance_t *inst);
void     imu_chip_take_pair_diag(imu_chip_instance_t *inst,
                                  uint32_t *reuse, uint32_t *unknown);
```
  This is the single module Tasks 5–10 add register-level logic to. `imu_driver.c`/`imu_driver_i2c.c` each keep only: device-add (`spi_bus_add_device` / `i2c_master_bus_add_device` via Task 12's `idl1_i2c_bus_get()`), `platform_read`/`platform_write`/`platform_mdelay`, a per-instance `stmdev_ctx_t` + `imu_chip_instance_t` array, and thin wrappers that call the `imu_chip_*` functions above.

**Tier: capable — explicit judgment row in rev2 (design of the shared-module boundary), not a mechanical transcription.**

- [ ] Step 1: Write `imu_chip_dsv320x.h` with the struct and function signatures above (this is the interface every later IMU task codes against — do not change these signatures in later tasks without updating this file first).
- [ ] Step 2: Move the init sequence, `diag_dump_one`-style diagnostic, FIFO-count/overrun read, and the burst-drain pairing loop out of both `imu_driver.c` and `imu_driver_i2c.c` into `imu_chip_dsv320x.c`, unifying the two nearly-identical copies into one, parameterized only by `stmdev_ctx_t *ctx` and `imu_chip_instance_t *inst` (no `spi_device_handle_t` or `i2c_master_dev_handle_t` reference inside this file — those stay in the two adapter files). Leave the register-value tables (ODR/FS/BDR conversion helpers, high-g enable, FIFO tag switch) as empty stub functions for now — Tasks 5–9 fill them in; this task is purely the file-split/dedupe shape.
- [ ] Step 3: Rewrite `imu_driver.c` to keep only `idl0_imu_init_one()` (adds SPI device, builds `stmdev_ctx_t`, calls `imu_chip_init()`), `idl0_imu_fifo_count()`, `idl0_imu_drain()`, `idl0_imu_check_overrun()`, `idl0_imu_take_pair_diag()` — each a one-line call into the matching `imu_chip_*` function plus the existing bounds/state checks. Do the same for `imu_driver_i2c.c`'s `_i2c` suffixed functions.
- [ ] Step 4: Build. There is no behavior yet to test on-device (the stub functions haven't been filled in) — confirm only that the project compiles with the new three-file split and that `imu_task.c`'s existing calls into `idl0_imu_init_one`/`idl0_imu_drain`/etc. still resolve unchanged (its call sites do not need to change — the public API surface of `imu_driver.h`/`imu_driver_i2c.h` stays the same).
- [ ] Step 5: Commit.

---

### Task 5: WHO_AM_I + reset/IF_CFG boot sequence (rev2 A1, A12; A-reg R1–R7)

**Files:**
- Modify: `main/imu_chip_dsv320x.c`, `main/imu_driver.h` (WHO_AM_I doc string)

**Interfaces:**
- Consumes: `imu_chip_init()` signature from Task 4.
- Produces: `IMU_WHO_AM_I` constant used by Task 14's pod probe.

**Tier: cheap — fully specified transcription.**

- [ ] Step 1: Set `#define IMU_WHO_AM_I 0x73` in `imu_chip_dsv320x.c` (was `0x6C`), and update the log string and the `imu_driver.h` doc comment that quotes it.
- [ ] Step 2: In `imu_chip_init()`, after the software-reset poll-until-clear loop (keep the existing 10×2ms pattern — neither datasheet specifies a completion time), add: **for the SPI (onboard) instance only**, write `IF_CFG` (03h) bit 0 `I2C_I3C_disable = 1`. This register is not cleared by `SW_RESET`, so it must be written every init, not assumed sticky. For pod I2C instances, leave `IF_CFG` at its power-on default (bit 0 = 0). Distinguish the two by a new `bool is_spi_bus` parameter — add it to `imu_chip_init()`'s signature: `bool imu_chip_init(stmdev_ctx_t *ctx, imu_chip_instance_t *inst, bool is_spi_bus, uint16_t sample_rate_hz, ...)`.
- [ ] Step 3: Confirm `BDU` (Block Data Update) is written to CTRL3 bit 6 = 1 explicitly (DSV320X reset default is already 1, DSO32's was 0 — write it anyway rather than relying on either default).
- [ ] Step 4: Change the gyro turn-on wait comment from "35 ms" to "40 ms" wherever cited (this doesn't gate any code path today — `main/` never blocks on it beyond the boot-order envelope Task 17 builds — but fix the number in the comment).
- [ ] Step 5: Flash to an onboard IMU (SPI) and confirm via serial log that WHO_AM_I reads back `0x73` and init logs `OK`, not `WHO_AM_I mismatch`.
- [ ] Step 6: Commit.

---

### Task 6: Low-g ODR/FS/BDR conversion tables + validation-reject (rev2 A4; A-reg R8, R10, R12, R14)

**Files:**
- Modify: `main/imu_chip_dsv320x.c`
- Modify: `main/idl0_config.c` (validation entry point)

**Interfaces:**
- Consumes: `imu_chip_instance_t` from Task 4.
- Produces: the four conversion helpers other tasks (7, 8) call: `xl_odr_enum(uint16_t hz)`, `gy_odr_enum(uint16_t hz)`, `xl_fs_enum(uint8_t g)`, `gy_fs_enum(uint16_t dps)` — each returns a driver enum on a valid input and is **never called with an off-list value** (validation rejects those before this point).

**Tier: cheap — every value is FINAL per decisions.md P2/P3.**

- [ ] Step 1: Rewrite `xl_odr_enum`/`gy_odr_enum` against CTRL1/CTRL2 bits[3:0] `ODR_XL`/`ODR_G`: 0001=1.875 Hz (accel only — no gyro code there), 0010=7.5, 0011=15, 0100=30, 0101=60, 0110=120, 0111=240, 1000=480, 1001=960, 1010=1.92k, 1011=3.84k, 1100=7.68k. There is **no `default:` fallback that silently maps an off-list value** — every caller of these functions must have already validated its input (Step 4).
- [ ] Step 2: Rewrite `xl_fs_enum` against CTRL8 (17h) bits[1:0] `FS_XL`: 00=±2g, 01=±4g, 10=±8g, 11=±16g. There is no 32g code on this register — that value only exists on the separate high-g path (Task 8).
- [ ] Step 3: Rewrite `gy_fs_enum` against CTRL6 (15h) bits[2:0] `FS_G`: 001=±250, 010=±500, 011=±1000, 100=±2000, 101=±4000 (000 reserved, no 125 dps code). CTRL6 bit 3 must be written 1 on every write to this register (read-modify-write, per A-reg R30 — never write a whole-byte constant copied from the DSO32 driver).
- [ ] Step 4: In `idl0_config.c`'s config-load/validate path (the same point `CONFIG_COMMIT` already runs through), add rejection for any `imu.sample_rate_hz` value not in `{7.5, 15, 30, 60, 120, 240, 480, 960, 1920, 3840, 7680}`, `imu.accel_range_g` not in `{2,4,8,16}`, or `imu.gyro_range_dps` not in `{250,500,1000,2000,4000}`: log the rejected value and the field name, and leave the previously-loaded valid value in place rather than falling through to a config write.
- [ ] Step 5: Write a host-compiled test in a new `test/test_imu_config_validate.c` (pattern matches `test/test_idl0_format.c` — `cc -std=c99 -I../main test_imu_config_validate.c ../main/idl0_config.c ... -o test_imu_config_validate`) asserting: a config JSON with `"sample_rate_hz": 833` (an old DSO32 value, now off-list) is rejected and the loaded struct's `imu_sample_rate_hz` is unchanged from its prior value, not silently set to 833 or remapped to 960.
- [ ] Step 6: Run the test, confirm PASS.
- [ ] Step 7: Commit.

---

### Task 7: Power-mode mapping + high-g interlock (rev2 A5, A18; A-reg R9, R11, R32)

**Files:**
- Modify: `main/imu_chip_dsv320x.c`

**Interfaces:**
- Consumes: `xl_fs_enum`/`gy_fs_enum` from Task 6, `hg_enabled` parameter from Task 4's `imu_chip_init()` signature.
- Produces: one shared interlock-check function, called from both the low-g config path and Task 8's high-g enable path:
```c
/* Returns the OP_MODE_XL value to actually write: if hg_enabled forces
 * high_performance semantics regardless of the caller's requested value,
 * logs a diag override. Never returns a value outside {0b000, 0b001}
 * when hg_enabled is true. */
uint8_t imu_chip_resolve_op_mode_xl(bool high_performance, bool hg_enabled);
```

**Tier: capable — explicit judgment row in rev2 for both the mapping choice and the interlock design.**

- [ ] Step 1: Replace the boolean-driven `xl_power_mode_set`/`gy_power_mode_set` calls with direct writes: `high_performance=true` → CTRL1 bits[6:4] `OP_MODE_XL` = `000` (high-performance) and CTRL2 bits[6:4] `OP_MODE_G` = `000`; `false` → `OP_MODE_XL` = `111` ("normal") and `OP_MODE_G` = `101` ("low-power"). Never write CTRL6 bit 4 or CTRL7 bit 7 (both repurposed on this chip — CTRL6 bit 4 is now `LPF1_G_BW`, CTRL7 bit 7 is now `INT1_DRDY_XL_HG`).
- [ ] Step 2: Implement `imu_chip_resolve_op_mode_xl()`: if `hg_enabled` is true and the caller requested `high_performance=false` (which would resolve to `111`), override to `000` and return it, and increment a diag counter / emit one `ESP_LOGW` per override (not per poll — this is an init-time event) reading `"IMU%u: hg enabled, overriding requested low-power accel mode to high-performance (datasheet Table 21 constraint)"`. If `high_performance=true` was requested, pass it through unchanged (`000`).
- [ ] Step 3: Call `imu_chip_resolve_op_mode_xl()` from `imu_chip_init()` (Task 4) at the single point that writes `OP_MODE_XL`, so the SPI and I2C adapters both get the interlock for free rather than re-checking it themselves — this is the "one function called from both config paths" rev2 A18 asks for.
- [ ] Step 4: Flash a pod with `imu.imuN.high_performance_mode = false` and `imu.hg_range_g`/`imu.hg_sample_rate_hz` set (high-g enabled). Confirm the serial log shows the override warning and that a register readback of CTRL1 bits[6:4] (via the existing diag burst-dump path) reads `000`, not `111`.
- [ ] Step 5: Commit.

---

### Task 8: High-g enable + FIFO batch bit (rev2 A16, A17; A-reg R13, R31)

**Files:**
- Modify: `main/imu_chip_dsv320x.c`, `main/imu_chip_dsv320x.h`
- Modify: `main/idl0_config.h` / `.c` (new fields — see Task 20, which this task's signature anticipates)

**Interfaces:**
- Consumes: `imu_chip_init()`'s `hg_enabled`, `hg_sample_rate_hz`, `hg_range_g` parameters (Task 4).
- Produces: `hg_odr_enum(uint16_t hz)`, `hg_fs_enum(uint16_t g)`, called by Task 9's drain loop indirectly via the sensitivity table it builds.

**Tier: cheap — every value is FINAL per decisions.md P1 and A-reg R13/R31; scope note: pods (IMU1/IMU2) only, per this plan's "Resolved ambiguities" — pass `hg_enabled = false` unconditionally for the onboard (IMU0) instance from `imu_task.c` regardless of config.**

- [ ] Step 1: Add `hg_odr_enum`: valid `ODR_XL_HG[2:0]` = 011=480 Hz, 100=960, 101=1.92k, 110=3.84k, 111=7.68k (000=power-down, 001/010 reserved — never written). Off-list values rejected at the same `idl0_config.c` validation point Task 6 established, valid set `{480, 960, 1920, 3840, 7680}`.
- [ ] Step 2: Add `hg_fs_enum`: valid `FS_XL_HG[2:0]` = 000=±32g, 001=±64g, 010=±128g, 011=±256g, 100=±320g. Valid set `{32, 64, 128, 256, 320}`.
- [ ] Step 3: In `imu_chip_init()`, when `hg_enabled` is true, write `CTRL1_XL_HG` (4Eh): bit `XL_HG_REGOUT_EN=1`, `ODR_XL_HG[2:0]` from Step 1, `FS_XL_HG[2:0]` from Step 2. When `hg_enabled` is false (always true for IMU0 per this task's scope note), leave `CTRL1_XL_HG` at its power-on-reset value (`XL_HG_REGOUT_EN=0`) — do not write the register at all.
- [ ] Step 4: When `hg_enabled` is true, also set `COUNTER_BDR_REG1` (0Bh) bit 4 `XL_HG_BATCH_EN = 1`. Add a comment directly at this write: `/* XL_HG_BATCH_EN is enable-only — there is no BDR_HG rate field. The high-g stream batches into FIFO at whatever ODR_XL_HG (above) is configured and cannot be decimated. Do not look for a batch-rate register here. */`
- [ ] Step 5: Flash a pod with `imu.imu1.hg_range_g=64`, `imu.imu1.hg_sample_rate_hz=480`. Confirm via the existing diag burst-dump (or a targeted register read added for this check) that `CTRL1_XL_HG` reads back `XL_HG_REGOUT_EN=1`, `FS_XL_HG=001`, `ODR_XL_HG=011`, and `COUNTER_BDR_REG1` bit 4 = 1.
- [ ] Step 6: Commit.

---

### Task 9: High-g record format + FIFO tag 0x1D decode (rev2 A19)

**Files:**
- Modify: `main/imu_chip_dsv320x.c`, `main/imu_chip_dsv320x.h` (the `imu_chip_drain()` signature from Task 4 already reserves `out_hg`/`max_hg`/`n_hg` parameters — this task fills them in)
- Modify: `main/idl0_format.c`, `main/idl0_format.h`

**Interfaces:**
- Consumes: `imu_chip_drain()` signature from Task 4.
- Produces:
```c
/* CHANNEL_SAMPLE (0x03) × 3, back-to-back, X/Y/Z on consecutive channel
 * IDs starting at base_channel_id (24 for IMU1, 27 for IMU2). Reuses the
 * existing generic per-channel scalar record — no new binary record type. */
size_t idl0_format_hg_accel_sample(uint8_t *out_buf, size_t out_buf_len,
                                    uint8_t base_channel_id,
                                    int64_t timestamp_us,
                                    const int16_t hg_accel[3]);
```

**Tier: capable — record-format design; see "Resolved ambiguities" above for the reasoning behind this specific shape.**

- [ ] Step 1: In `imu_chip_dsv320x.c`'s burst-drain parse loop (the same loop that already switches on `tag = word[0] >> 3` for `GYRO_NC_TAG`/`XL_NC_TAG`), add an `else if (tag == 0x1D)` branch: this tag carries a self-contained X/Y/Z triplet (no pairing needed — one FIFO word = one complete high-g sample, unlike gyro/accel). Copy the 6 data bytes into `out_hg[n_hg]`, increment `n_hg`, bounded by `max_hg`. This branch runs regardless of `s_have_gyro` state — it never touches the gyro/accel pair-builder.
- [ ] Step 2: Implement `idl0_format_hg_accel_sample()` in `idl0_format.c` as three calls to the existing `idl0_format_channel_sample(out_buf + offset, base_channel_id + axis, timestamp_us, &hg_accel[axis], sizeof(int16_t))` for `axis` in `{0,1,2}` (X→base, Y→base+1, Z→base+2), returning the summed byte count. Bounds-check `out_buf_len` against 3× the per-record size before writing.
- [ ] Step 3: In `imu_task.c`'s `drain_one()`, after the existing gyro/accel batch-format loop, add a second loop over the `n_hg` triplets returned by `imu_chip_drain()`: compute each triplet's timestamp via its own walk-back grid using `hg_period_us = 1000000LL / cfg.hg_sample_rate_hz[imu_index]` (not the shared low-g `period_us` — high-g can run at a different rate), clamp against a **separate** per-IMU `s_last_hg_ts[imu_index]` monotonic guard (do not share `s_last_ts[]` with the low-g/gyro stream — they are different channels with independent monotonicity requirements), and call `idl0_format_hg_accel_sample()` with `base_channel_id = 24 + (imu_index - 1) * 3` (imu_index 1→24, 2→27 — this expression is only ever evaluated for pod indices per Task 8's scope note).
- [ ] Step 4: Write a host-compiled test `test/test_hg_format.c` asserting `idl0_format_hg_accel_sample()` with `base_channel_id=24` produces three `CHANNEL_SAMPLE` records with `channel_id` 24, 25, 26 in order, each carrying the correct axis value, by decoding the output buffer with the same little-endian readers `test_idl0_format.c` already defines.
- [ ] Step 5: Run the test, confirm PASS.
- [ ] Step 6: Commit.

---

### Task 10: High-g registry channels 24–29 + FIFO burst-constant/comment fix (rev2 A6, A20, A21)

**Files:**
- Modify: `main/imu_task.c` (registry entry construction)
- Modify: `main/imu_chip_dsv320x.c` (burst constant comment)

**Interfaces:**
- Consumes: `idl0_channel_registry_entry_t` (existing, from `idl0_format.h`), `idl0_format_hg_accel_sample()` from Task 9.

**Tier: cheap — values fully specified once Task 9's format function exists.**

- [ ] Step 1: Wherever the session-start channel registry is built (the code that already emits `IMUx_Accel*`/`IMUx_Gyro*` entries for channels 0–17), add six entries for an enabled pod with `hg_enabled`: `channel_id` 24–29, `name` = `"IMU1_HgAccelX"` … `"IMU2_HgAccelZ"`, `data_type` = 4 (i16), `sample_rate_hz` = the pod's configured `hg_sample_rate_hz`, `units` = the same units string the existing low-g accel channels use, `scale` = `hg_range_g / 32768.0f` using whichever `FS_XL_HG` value is configured (±32g→0.976 mg/LSB … ±320g→10.417 mg/LSB, matching the existing `range/32768` pattern spec §5.2 already uses for low-g), `offset` = 0.
- [ ] Step 2: Only emit these six entries when that pod's `imu.imuN.enabled` **and** high-g is enabled for it — a pod with high-g off must not carry dead registry rows (mirrors the existing rule that a maskless IMU emits no registry entries at all).
- [ ] Step 3: Correct the three comments in `imu_chip_dsv320x.c` that attribute the 512-word / 4.5 kB FIFO burst constant to a "3 KB DSO32 FIFO" — it is now exactly the DSV320X's real FIFO size, not a coincidence. Cap the FIFO watermark (if the driver ever sets one) at 255 words, not 511 — DSV320X's watermark field narrowed to 8 bits (CTRL2 bit 0 is now reserved, was part of a 9-bit field on DSO32).
- [ ] Step 4: Add a one-line doc comment near the burst-drain code: the high-g stream is a third 7-byte-tagged-word producer per pod FIFO at its own non-decimatable rate; it still fits the 512-word FIFO without a depth change, but it's the dominant new term in the shared pod I2C bus budget (cross-reference Task 16's bus-budget note).
- [ ] Step 5: Flash a pod, start a session, confirm the on-SD session header (or a debug dump of the registry array before it's written) contains channels 24–29 with the expected names/scale when high-g is enabled, and does not contain them when it isn't.
- [ ] Step 6: Commit.

---

### Task 11: `imu_driver.h` contract update (rev2 A8)

**Files:**
- Modify: `main/imu_driver.h`

**Tier: cheap.**

- [ ] Step 1: Update the `idl0_imu_init_one()` doc comment: `accel_range_g` "4, 8, 16, or 32" → "2, 4, 8, or 16"; `gyro_range_dps` "125 … 2000" → "250, 500, 1000, 2000, or 4000"; WHO_AM_I `0x6C` → `0x73`.
- [ ] Step 2: Add an explicit sentence: "Off-list values are rejected at config-validation time, not mapped to the nearest supported value — this is a documented contract, not just internal behavior (decisions.md P2/P3)."
- [ ] Step 3: Add a doc note next to `idl0_imu_sample_t` cross-referencing where high-g data lands instead (it is **not** a field on this struct — see `idl0_format_hg_accel_sample()` in `idl0_format.h`, Task 9).
- [ ] Step 4: Commit.

---

### Task 12: I2C bus speed + bus-owner refactor (rev2 A9, D2)

**Files:**
- Create: `main/i2c_bus.c`, `main/i2c_bus.h`
- Modify: `main/imu_driver_i2c.c` (remove private `s_bus`)

**Interfaces:**
- Produces:
```c
/* Lazily creates the shared pod I2C bus (SDA/SCL from pins.h, 400 kHz,
 * internal pull-ups disabled) on first call; returns the same handle on
 * every subsequent call. Returns NULL on i2c_new_master_bus() failure. */
i2c_master_bus_handle_t idl1_i2c_bus_get(void);
```
  Consumed by `imu_driver_i2c.c` (Task 4's adapter) and `components/tla2528/tla2528.c` (Task 21).

**Tier: capable — small refactor, explicit judgment row in rev2 (D2), plus the already-FINAL bus-speed decision to wire in.**

- [ ] Step 1: Create `main/i2c_bus.c` implementing `idl1_i2c_bus_get()`: on first call, build `i2c_master_bus_config_t` with `.i2c_port = I2C_NUM_0`, `.sda_io_num = IDL1_PIN_I2C_SDA`, `.scl_io_num = IDL1_PIN_I2C_SCL`, `.clk_source = I2C_CLK_SRC_DEFAULT`, `.glitch_ignore_cnt = 7`, `.flags.enable_internal_pullup = false`. Log `"pod I2C bus up: SDA=GPIO%d SCL=GPIO%d @ 400000 Hz"`. Cache the handle in a static.
- [ ] Step 2: In `imu_driver_i2c.c`, delete the private `s_bus` static and its `i2c_bus_init()` function; replace every use with a call to `idl1_i2c_bus_get()`. Set each pod device's `i2c_device_config_t.scl_speed_hz = 400000` (was `I2C_BUS_HZ = 1000000` — DSV320X supports 1 MHz but the ESP32-C6 controller doesn't, per its datasheet §4.2.1.3; this is FINAL, not a bring-up measurement item).
- [ ] Step 3: Delete the now-stale comment in `imu_driver_i2c.c` claiming "LSM6DSO32 supports it [1 MHz] per datasheet" — that was never true for either chip on this host controller.
- [ ] Step 4: Build. Flash a pod-equipped board and confirm both pod IMUs and (once Task 21 lands) the TLA2528 enumerate on the same bus handle — check via serial log that `idl1_i2c_bus_get()`'s "bus up" log line prints exactly once per boot, not once per device.
- [ ] Step 5: Commit.

---

### Task 13: `pins_idl1.h` real GPIO values (rev2 A10)

**Files:**
- Modify: `main/pins_idl1.h`

**Interfaces:**
- Produces: `IDL1_PIN_I2C_SDA`, `IDL1_PIN_I2C_SCL`, `IDL1_PIN_CS_IMU_ONBOARD` (consumed by Task 12/Task 4's SPI adapter).

**Tier: cheap — transcription. The full header content already exists, reviewed and namespace-audited: transcribe it, do not re-derive it.**

- [ ] Step 1: Copy the complete `pins_idl1.h` from
  `C:/Users/isaac/Documents/Saucy/saucyeng/idl1-program/findings/2026-09-02-pins-idl1/pins_idl1-spec.md` §3 — the finalized file content (15 `IDL1_PIN_*` GPIOs, 6 `IDL1_TLA_CH_*` channels, 3 `IDL1_I2C_ADDR_*` constants, plus the neutral `IDL_PIN_*` alias block for the 10 shared-semantics signals). Key values, netlist-confirmed by both `pins-delta.md` and the spec: SDA=22, SCL=23, onboard IMU CS=21, SPI SCK/MOSI/MISO=19/18/20 (unchanged), **CS_SD=GPIO_NUM_7** (was 23; 23 is SCL now), PWR_EN=2.
- [ ] Step 2: Verify the copied header compiles standalone and that no `IDL1_TLA_CH_*` token expands to a `GPIO_NUM_*` value (the spec's §5 namespace audit lists the four live digit collisions to check, GPIO7/SD_CS being the load-bearing one).
- [ ] Step 3: Commit.

---

### Task 14: Pod I2C address table + boot probe (rev2 A11, D3)

**Files:**
- Modify: `main/imu_task.c`

**Interfaces:**
- Consumes: `IMU_WHO_AM_I` (0x73) from Task 5.

**Tier: cheap — values FINAL per decisions.md P7.**

- [ ] Step 1: Update `s_imu_i2c_addr[]`: index 1 (front, spec IMU1) = `0x6A`, index 2 (rear, spec IMU2) = `0x6B`. Re-comment: "front pod = JP1 open = 0x6A = spec IMU1; rear pod = JP1 closed = 0x6B = spec IMU2 (decisions.md P7, FINAL)."
- [ ] Step 2: At boot, for each enabled pod, attempt init at its fixed address and log whichever addresses answered WHO_AM_I=0x73 and which didn't. An absent pod (hardtail — no rear pod fitted) is a normal, expected condition per spec §3.2 — log it at `ESP_LOGI`, not `ESP_LOGE` or `ESP_LOGW`.
- [ ] Step 3: No change to `imu_task.c`'s existing per-pod independent-init shape — this task only corrects the address/comment mapping, it does not add scanning (there is nothing to scan; each pod has one fixed, known address).
- [ ] Step 4: Flash with only the front pod fitted, confirm serial log shows front (0x6A) up and rear (0x6B) logged as absent/expected, not as an error.
- [ ] Step 5: Commit.

---

### Task 15: Bus-split rationale comment cleanup (rev2 A13)

**Files:**
- Modify: `main/imu_driver.h`, `main/imu_task.c`, `main/pins_idl1.h`

**Tier: cheap.**

- [ ] Step 1: Replace every comment that explains the SPI/I2C split via the Adafruit-breakout level-shifter story (the "TEMP: I²C bus" framing) with the IDL1 rationale: one chip type (LSM6DSV320X) run over two buses by design — onboard on SPI (shares the bus with the SD card), pods on I2C (shared with the TLA2528), not a workaround for a broken level shifter.
- [ ] Step 2: Remove the "Remove this table... once the external boards are reworked" framing from `imu_task.c`'s `s_imu_bus[]` comment — on IDL1 this dispatch table is permanent, not temporary.
- [ ] Step 3: Commit.

---

### Task 16: Default ODR wiring + FIFO-overrun diag instrumentation (rev2 A14; A-reg R21; decisions.md P2's day-one mandate)

**Files:**
- Modify: `main/imu_task.c`
- Modify: `main/idl0_config.c` (default-value table)

**Interfaces:**
- Consumes: Task 6's validation point.

**Tier: cheap — values are FINAL; this is wiring the fallback to the config-default table rather than a literal, plus turning on existing overrun-diag plumbing for the pod bus specifically.**

- [ ] Step 1: Replace the bare `s_odr_hz = 104` fallback literal (or its DSV320X-era equivalent) with a lookup into the same config-default table `idl0_config_load()` already falls back to on a missing/corrupt config file — for the onboard IMU that default is **960 Hz**, for pods it is **480 Hz** (decisions.md P2 + rev2 D4's recomputed bus-budget verdict — 960 Hz stays a valid config value to try at bring-up, but the shipped default is 480). This must be a config-driven default, not a second hardcoded literal living next to the one Task 6 already validates against.
- [ ] Step 2: Confirm (it already exists per `driver-impact.md`'s "Data-ready without INT" section) that `imu_chip_fifo_count()` (Task 4) reads and returns the `FIFO_STATUS1/2` overrun bit on every poll, and that `imu_task.c`'s `drain_one()` already increments `s_diag_overruns[imu_index]` / `s_m_overruns[imu_index]` on it. This is already wired for the existing gyro/accel path — verify it fires for pods specifically (not just the onboard IMU) by checking the periodic `emit_diag_log()` line includes all three IMU indices, which it already does.
- [ ] Step 3: Add one line to the session-end diag flush (`idl0_imu_diag_flush()`) explicitly calling out the pod ODR in effect, so a field log makes it obvious at a glance whether a session ran the 480 Hz default or an operator-raised 960 Hz: extend the existing `idl0_diag_log_event("IMU%u %s drains=...")` line with ` odr_hz=%u hg_odr_hz=%u` fields sourced from the same per-IMU config Task 8 already threads through.
- [ ] Step 4: Flash a pod at the 480 Hz default, run a short session, confirm the SD diag log (`idl0_debug.log`) shows `ovr=0` and `odr_hz=480`. This is the bring-up measurement decisions.md P2 requires before 960 Hz can ever be considered safe to ship as a default — this task only wires the instrumentation, it does not run or interpret that measurement.
- [ ] Step 5: Commit.

---

### Task 17: Boot/power sequencing — PWR_EN (decisions.md H1/H2 boot-survival note; rev2 D1)

**Files:**
- Modify: `main/pins_idl1.h` (add `IDL1_PIN_PWR_EN`)
- Modify: `main/main.c` (or wherever `app_main()` lives)

**Interfaces:**
- Produces: the boot-time GPIO state every pod/analog init task (14, 21) depends on being already stable before it runs.

**Tier: cheap — order and the specific pin are FINAL; the wait margin is a fixed boot-time constant, not a runtime-configurable sensor value (the "every rate/range is config" rule in this plan's Global Constraints is scoped to sensor ODR/range/OSR, not boot-sequencing delays).**

- [ ] Step 1: Add to `pins_idl1.h`: `#define IDL1_PIN_PWR_EN GPIO_NUM_2`.
- [ ] Step 2: At the very start of `app_main()`, before any SPI/I2C bus init, configure `IDL1_PIN_PWR_EN` as `GPIO_MODE_OUTPUT` and drive it high immediately. **Never** reconfigure this pin as an input or call any function that could revert its direction, at any point in the firmware's lifetime (including any future OTA/deep-sleep/error-recovery path) — a working board depends on this switched rail staying enabled for the SD card write path too (TLA2528 GPOs are the only board LEDs, per H1, so the LEDs going dark is not itself a fault signal, but everything downstream of PWR_EN — both pods, the TLA2528, and the wheel-speed pull-ups — silently going dead is).
- [ ] Step 3: After driving `PWR_EN` high, wait **50 ms** (a rounded envelope covering: TPS22919 rail rise, DSV320X boot, gyro turn-on 40 ms, ATS601 ≤1 ms, TLA2528 tPU ≤5 ms) before attempting any WHO_AM_I read or TLA2528 SYSTEM_STATUS read.
- [ ] Step 4: Only after that wait, proceed to: onboard IMU SPI WHO_AM_I (Task 5) → pod I2C WHO_AM_I probes at 0x6A/0x6B (Task 14) → TLA2528 SYSTEM_STATUS probe at 0x10 (Task 21). Each is independent — a failure at any step does not block the next (boot must survive the ADC absent, per decisions.md H2).
- [ ] Step 5: Flash and confirm via serial log timestamps that `PWR_EN` goes high before the first I2C transaction attempt, and that the gap between them is ≥50 ms.
- [ ] Step 6: Commit.

---

### Task 18: Wheel speed pins + registry (rev2 B1, B2, B3, B5, B6)

**Files:**
- Modify: `main/pins_idl1.h`
- Modify: `main/idl0_config.h` / `.c` (wheel-speed channel config, if not already generic enough)

**Interfaces:**
- Produces: `IDL1_PIN_SPEED_FRONT`, `IDL1_PIN_SPEED_REAR`, consumed by Task 19.

**Tier: cheap — netlist-fixed values, unaffected by any decisions.md correction.**

- [ ] Step 1: Add to `pins_idl1.h`:
```c
#define IDL1_PIN_SPEED_FRONT       GPIO_NUM_0
#define IDL1_PIN_SPEED_REAR        GPIO_NUM_1
```
- [ ] Step 2: Registry: channel 18 = `WheelFront`, channel 19 = `WheelRear`, `units = "pulse"`, `scale = 1.0`, `offset = 0`, `data_type` = u32 (2), `sample_rate_hz = 0` (event-driven). Emit only when `wheel_speed.<slot>.enabled` in config.
- [ ] Step 3: GPIO config for both pins: input mode, **no internal pull** (external 4k7 to +3V3 on the main board already provide it), interrupt on **falling edge** (ATS601 output is open-drain, idle high, pulls low on a tooth). Confirm GPIO0/GPIO1 are not strapping pins on the C6 (strapping pins are GPIO4/5/8/9/15 — these two are clear).
- [ ] Step 4: One `CHANNEL_SAMPLE` (0x03) per pulse: `u32` monotonic pulse count, timestamp from `esp_timer_get_time()` taken in the ISR (not the consuming task — jitter matters at ~725 Hz tooth rate). Speed is computed app-side from the pulse stream; never on-device.
- [ ] Step 5: Commit (pairs with Task 19, which adds the actual ISR wiring — this task establishes the pin/registry facts Task 19 codes against).

---

### Task 19: `digital_task.c` wheel-kind extension + debounce guard (rev2 B4, B7)

**Files:**
- Modify: `main/digital_task.h`, `main/digital_task.c`

**Interfaces:**
- Consumes: `IDL1_PIN_SPEED_FRONT`/`_REAR` from Task 18, `idl0_format_channel_sample()` (existing).
- Produces:
```c
typedef enum { DIGITAL_KIND_MARKER, DIGITAL_KIND_WHEEL } digital_kind_t;

typedef struct {
    bool           enabled;
    digital_kind_t kind;          /* new field */
    int            gpio_pin;
    bool           active_low;
    uint16_t       debounce_ms;   /* ignored for kind=WHEEL; see min_interval_us below */
    uint32_t       min_interval_us; /* new — WHEEL only: reject an edge this soon after the last */
    char           label[20];
    uint8_t        channel_id;
} digital_channel_cfg_t;
```

**Tier: capable — explicit judgment rows in rev2 (B4's specific guard number, B7's small extension design).**

- [ ] Step 1: Add `digital_kind_t` and the two new fields to `digital_channel_cfg_t` as shown above. Existing marker channels get `kind = DIGITAL_KIND_MARKER`, `min_interval_us` unused (0).
- [ ] Step 2: In `process_press_event()` (or a new `process_wheel_event()` called alongside it), branch on `kind`: `DIGITAL_KIND_MARKER` keeps today's `debounce_ms`-based reject-and-count-with-u8-wraparound behavior unchanged. `DIGITAL_KIND_WHEEL` instead rejects an edge if `(ev->timestamp_us - s_last_press_us[idx]) < ch->min_interval_us`, uses a **`u32`** monotonic counter (not the existing `u8`), and calls `idl0_format_channel_sample()` with that 4-byte value instead of 1.
- [ ] Step 3: Set `min_interval_us = 200` (200 µs) for both wheel channels at config-load time — this rejects anything above ~5 kHz, well above any plausible wheel rate (a 60-tooth ring at 12 rev/s is ≈725 Hz), while the 4k7+10nF network's ≈100 µs edge (τ=47 µs) means a clean single edge per tooth is expected well under this threshold. Do **not** reuse the existing 1 ms `debounce_ms` default for wheel channels — it is too coarse for a 725 Hz pulse train (it would drop real pulses, not just noise).
- [ ] Step 4: In `digital_task_start()`, when configuring the GPIO for a `kind=WHEEL` channel, set `pull_up_en = GPIO_PULLUP_DISABLE` / `pull_down_en = GPIO_PULLDOWN_DISABLE` regardless of `active_low` (unlike markers, which use internal pulls) — Task 18 already established external 4k7 pull-ups exist on this signal.
- [ ] Step 5: Write a host-compiled test (or, if the ISR/GPIO layer can't be host-tested, a targeted on-device check): feed two synthetic wheel events 150 µs apart and confirm the second is rejected (counter does not increment); feed two events 250 µs apart and confirm both are counted.
- [ ] Step 6: Flash with a wheel sensor (or a signal generator on the pin) and confirm via serial/SD log that pulse counts increment monotonically and roughly track the expected tooth rate.
- [ ] Step 7: Commit.

---

### Task 20: Config schema — `imu.hg_range_g`, `imu.hg_sample_rate_hz`, `analog.osr` (decisions.md Q8/Q9, resolved FINAL)

**Files:**
- Modify: `main/idl0_config.h`, `main/idl0_config.c`

**Interfaces:**
- Produces: the config fields Tasks 8 and 21/23 read.

**Tier: cheap — field names are FINAL per decisions.md's Q8/Q9 resolution (this plan does not carry them forward as open, unlike rev2, which predates that resolution).**

- [ ] Step 1: Add to `idl0_config_t`, mirroring the existing `accel_range_g[3]`/`gyro_range_dps[3]` per-IMU-array pattern exactly (including the existing "top-level value seeds all three slots before per-IMU override" convention):
```c
/* High-g accelerometer (pods only — decisions.md P1/Q8). Index 0 (onboard)
 * is present for array-shape consistency with accel_range_g/gyro_range_dps
 * but is never read; onboard high-g is out of scope (see plan header). */
uint16_t hg_range_g[3];        /* valid {32,64,128,256,320}, default 64 */
uint16_t hg_sample_rate_hz[3]; /* valid {480,960,1920,3840,7680}, default 480 */
bool     hg_enabled[3];        /* mirrors imu_enabled but index 0 forced false */
```
  Config source: `imu.hg_range_g` / `imu.hg_sample_rate_hz` (top-level, seeds all slots) and `imu.imuN.hg_range_g` / `imu.imuN.hg_sample_rate_hz` (per-IMU override) — same JSON shape as the existing accel/gyro fields.
- [ ] Step 2: Add:
```c
uint8_t analog_osr;  /* valid {0,2,4,8,16,32,64,128}, default 0. Config source: analog.osr */
```
- [ ] Step 3: In `idl0_config_load()`'s validation pass (same point as Task 6), reject-and-keep-prior for any `hg_range_g`/`hg_sample_rate_hz`/`analog_osr` value outside its valid set, following the exact pattern Task 6 established.
- [ ] Step 4: Force `hg_enabled[0] = false` unconditionally after loading, regardless of what the JSON says for `imu.imu0.hg_range_g` etc. — this is the code-level enforcement of this plan's "onboard has no high-g" resolution, not just a doc note.
- [ ] Step 5: Extend `test/test_idl0_format.c`'s sibling config test (or add one) asserting a config with `"hg_range_g": 100` (off-list) is rejected and the prior default (64) survives.
- [ ] Step 6: Run the test, confirm PASS.
- [ ] Step 7: Commit.

---

### Task 21: TLA2528 driver — corrected addressing + init sequence (rev2 C2 mechanism, C10, C11; H2's address correction)

**Files:**
- Create: `components/tla2528/tla2528.c`, `components/tla2528/tla2528.h`, `components/tla2528/CMakeLists.txt`
- Modify: `main/pins_idl1.h` (TLA2528 channel constants)
- Modify: `main/CMakeLists.txt` (add `tla2528` to `REQUIRES` under `CONFIG_IDL_BOARD_IDL1`)

**Interfaces:**
- Consumes: `idl1_i2c_bus_get()` from Task 12.
- Produces:
```c
bool tla2528_init(void);   /* probes IDL1_TLA2528_ADDR_EXPECTED, configures
                             * auto-sequence over AIN4/AIN5, returns false
                             * (loud-logged) if the ADC doesn't answer — the
                             * caller must proceed to boot regardless */
bool tla2528_is_up(void);
```

**Tier: cheap — every register value here is FINAL and unaffected by the read-path correction (that's Task 23); the address correction itself is a single named constant, not a design decision.**

- [ ] Step 1: Add to `pins_idl1.h`: `#define IDL1_TLA2528_ADDR_EXPECTED 0x10` — the single named constant decisions.md H2 mandates ("probes exactly one expected address... trivial to change while testing candidates"). Do **not** use 0x17 anywhere in this component — that value is stale (see this plan's "Resolved ambiguities").
- [ ] Step 2: `tla2528_init()`: add the device to `idl1_i2c_bus_get()`'s handle at `IDL1_TLA2528_ADDR_EXPECTED`, `scl_speed_hz = 400000`. Read `SYSTEM_STATUS` (0x0, single-register-read opcode `0x10` per `tla2528-regmap.md`'s I2C protocol section: `S [addr W] A [0x10] A [0x0] A P/Sr S [addr R] A [data] A P/Sr`). **If this read fails (NACK/timeout), log a distinct, loud `ESP_LOGE` — e.g. `"TLA2528 not responding at 0x%02X (expected) — pod analog input disabled this boot"` — set an internal "absent" flag, and return `false`.** The caller (Task 17's boot sequence) must proceed regardless; both board LEDs are TLA2528 GPOs (H1), so a non-responding ADC also means no board LED indication — the onboard GPIO15 LED (already unaffected by this bus) is the only signal in that state, and this driver does not touch it.
- [ ] Step 3: If the probe succeeds, write `AUTO_SEQ_CH_SEL` (0x12) = `0x30` (`0b00110000` — bits 5,4 = AIN5, AIN4) via single-register-write opcode `0x08`: `S [addr W] A [0x08] A [0x12] A [0x30] A P`.
- [ ] Step 4: Write `OPMODE_CFG` (0x4) = `0x00` (`OSC_SEL=0` high-speed oscillator, `CLK_DIV[3:0]=0000b`). Comment directly at this write, verbatim per decisions.md: `/* Load-bearing, not tidy: at OSC_SEL=0/CLK_DIV=0000b a conversion cycle is ~1us, so even OSR=128 stretches ~128us. At OSC_SEL=1/CLK_DIV=1111b the same average holds the bus for 786ms. Do not "optimise for power" here. */`
- [ ] Step 5: Write `DATA_CFG` (0x2) = `0x10` (`APPEND_STATUS[1:0]=01b` at bits[5:4]) so the 4-bit channel ID rides with each conversion result — cheap insurance the sequencer is on the channel firmware expects, per decisions.md's stated rationale. (Task 23's read path treats this field as a cross-check, not primary routing — see Task 23 and Open Question 2.)
- [ ] Step 6: Write `OSR_CFG` (0x3) from `cfg.analog_osr` (Task 20) via the table `{0→0b000, 2→0b001, 4→0b010, 8→0b011, 16→0b100, 32→0b101, 64→0b110, 128→0b111}`.
- [ ] Step 7: Start the sequencer: write `SEQUENCE_CFG` (0x10) = `0x01` (`SEQ_MODE[1:0]=01b`, `SEQ_START=0`), then a second write `SEQUENCE_CFG = 0x11` (`SEQ_MODE=01b`, `SEQ_START=1`) — two writes, not combined, so the mode is committed before sequencing starts.
- [ ] Step 8: Add `IDL1_TLA_CH_ANALOG_FRONT_AIN5 = 5` and `IDL1_TLA_CH_ANALOG_REAR_AIN4 = 4` to `pins_idl1.h` under the `IDL1_TLA_CH_*` namespace (per Task 1's header comment — never mix these with `GPIO_NUM_*` values; channel 7 of this device is unrelated to `GPIO_NUM_7`).
- [ ] Step 9: Flash with NT1 opened (0x10 reachable) and confirm via serial log: probe succeeds, `SEQUENCE_CFG`/`AUTO_SEQ_CH_SEL`/`OPMODE_CFG`/`DATA_CFG` read back as written. Then flash with NT1 **not** yet opened (current board state) and confirm the loud failure log fires and the rest of boot (onboard IMU, pods, wheel speed) still comes up — this is the concrete "boot survives the ADC absent" check H2 mandates.
- [ ] Step 10: Commit.

---

### Task 22: TLA2528 LED GPIO wiring + collision guard (rev2 C4; decisions.md H1)

**Files:**
- Modify: `components/tla2528/tla2528.c`, `components/tla2528/tla2528.h`
- Modify: `main/led_status.c`
- Modify: `main/pins_idl1.h`

**Interfaces:**
- Consumes: `tla2528_init()`/`tla2528_is_up()` from Task 21.
- Produces:
```c
void tla2528_led_set(bool red_on, bool green_on);  /* no-op if !tla2528_is_up() */
```

**Tier: capable — explicit judgment row in rev2; the collision hazard with `AUTO_SEQ_CH_SEL` is a real correctness risk if gotten wrong (kills LEDs or turns the analog inputs into GPIOs).**

- [ ] Step 1: Add `IDL1_TLA_CH_LED_RED = 6` and `IDL1_TLA_CH_LED_GREEN = 7` to `pins_idl1.h` (H1: red = D1 = recording, green = D2 = power/BLE connection).
- [ ] Step 2: In `tla2528_init()` (Task 21), **after** `AUTO_SEQ_CH_SEL` is written (Step 3 of Task 21) — order matters, see Step 3 below — write `PIN_CFG` (0x5) = `0xC0` (`0b11000000`, bits 7/6 = channels 7/6 configured as GPIO; bits 5/4 = channels 5/4 stay analog input, matching `AUTO_SEQ_CH_SEL`'s `0x30`), `GPIO_CFG` (0x7) = `0xC0` (bits 7/6 = output direction), `GPO_DRIVE_CFG` (0x9) = `0xC0` (bits 7/6 = push-pull, not open-drain — these drive LEDs directly, no external pull expected).
- [ ] Step 3: **Explicit collision guard**, stated as a code comment and a debug-build assertion: `PIN_CFG` bits 4/5 must never be set to 1 (which would turn AIN4/AIN5 into GPIOs) and `AUTO_SEQ_CH_SEL` bits 6/7 must never be set to 1 (which would feed the LED-driving channels 6/7 into the analog sequencer). Add `assert(((pin_cfg_value >> 4) & 0x3) == 0)` and `assert(((auto_seq_value >> 6) & 0x3) == 0)` right before each write in a debug build, so a future edit to either constant that violates this trips immediately rather than silently killing LEDs or corrupting analog reads.
- [ ] Step 4: Implement `tla2528_led_set(bool red_on, bool green_on)`: writes `GPO_VALUE` (0xB) with bit 6 = `red_on`, bit 7 = `green_on`, leaving other bits unchanged (read-modify-write, since this register is shared conceptually with nothing else but should still not clobber reserved bits). No-op (return immediately) if `!tla2528_is_up()` — this is the concrete behavior for "both board LEDs sit behind I2C... nothing can indicate state until the bus and the TLA2528 are up" (H1's consequence note); the caller (Task 17's boot sequence, or wherever `led_status.c` already runs) falls back to the onboard GPIO15 LED for that window, which this task does not change.
- [ ] Step 5: In `led_status.c`, replace the direct `GPIO_NUM_9`/whatever XIAO-pin LED drive for the D1/D2 pair with calls to `tla2528_led_set()`. Keep the existing onboard-LED (GPIO15, active-low, `IDL1_PIN_LED_ONBOARD`) logic entirely separate — it covers boot and hard-fault states that predate the TLA2528 coming up.
- [ ] Step 6: Flash and visually confirm: green LED reflects power/BLE-connection state, red LED reflects recording state, and toggling recording does not perturb analog readings (verify once Task 23 lands — note this as a cross-task check, not blocking this task's completion).
- [ ] Step 7: Commit.

---

### Task 23: TLA2528 analog read path — corrected architecture (rev2 C3/C5/C12, redesigned; C6, C9 transcribed)

**Files:**
- Create: `main/analog_task.c`, `main/analog_task.h`
- Modify: `components/tla2528/tla2528.c`, `.h`

**Interfaces:**
- Consumes: `tla2528_is_up()` from Task 21, `cfg.analog_sample_rate_hz` / `cfg.analog_osr` from Task 20 (existing field + new field).
- Produces: channel 20 (`PressureFront`, AIN5), channel 21 (`PressureRear`, AIN4) `CHANNEL_SAMPLE` records.

**Tier: capable — this is the task that redesigns rev2's Part C read path against the corrected TLA2528 architecture; see this plan's "Resolved ambiguities" for why rev2's C3/C5/C12 are not transcribed as written.**

This task does **not** implement "read `RECENT_CHx_LSB/MSB`" — that register does not exist (decisions.md's TLA2528 correction, confirmed independently by `tla2528-regmap.md`'s real register table stopping at 0x12). The real mechanism, per decisions.md's corrected P5 ruling ("auto-sequence mode over AIN4/AIN5, host-initiated conversions, host reads the result") and `tla2528-regmap.md`'s "Reading Conversion Data" sections: a **bare I2C read frame with no register-address byte** (`S [addr R] A [data] A P`) both triggers a conversion at the sequencer's current channel and returns the *previous* conversion's result, then the sequencer advances. Per `SEQUENCE_CFG`'s own documented behavior ("Start auto sequencing from first enabled analog input channel starting from channel ID = 0, ascending order"), with only AIN4 and AIN5 enabled the sequence is deterministic: AIN4, AIN5, AIN4, AIN5, ... This plan uses that determinism as the **primary** channel-routing mechanism, with the `APPEND_STATUS` channel-ID field as a live cross-check — its layout is now confirmed (see "Open questions" resolution 2 at the top): at OSR=0 the ID occupies the low nibble of byte 1 (2-byte frame C); at OSR>0 it arrives as a third byte, high nibble (3-byte frame D). Read length is therefore `(cfg.analog_osr > 0) ? 3 : 2` bytes when APPEND_STATUS is enabled.

- [ ] Step 1: In `tla2528.c`, add:
```c
/* Bare I2C read, no register pointer — triggers/advances the sequencer's
 * conversion and returns the PREVIOUS conversion's result. Per Figure 25
 * (SBAS961A p.17): value is big-endian, left-justified — frame A (OSR=0)
 * is D11..D4 | D3..D0 0000, frame B (OSR>0) is D15..D0 — so
 * (b0<<8)|b1 is the correct 16-bit value at EVERY OSR setting. With
 * APPEND_STATUS: OSR=0 puts the 4-bit channel ID in b1's low nibble
 * (mask it out of the value); OSR>0 appends it as a 3rd byte, high
 * nibble. */
bool tla2528_read_raw(uint16_t *out_value, uint8_t *out_chan_id);
```
  Implemented as `i2c_master_receive(dev, buf, (cfg.analog_osr > 0) ? 3 : 2, I2C_TIMEOUT_MS)` (no preceding register-address transmit) with an IDF transaction timeout of **≥10 ms** (insurance against the documented clock-stretch behavior — Task 21's `OPMODE_CFG=0x00` keeps a full OSR=128 average under ~128 µs, but 10 ms is cheap margin against misconfiguration; note frame D documents stretching on the 3-byte read). Decode: `raw16 = (buf[0] << 8) | buf[1]`; at OSR=0, `*out_value = raw16 & 0xFFF0` and `*out_chan_id = buf[1] & 0x0F`; at OSR>0, `*out_value = raw16` and `*out_chan_id = buf[2] >> 4`. The left-justified OSR=0 value keeps one full-scale interpretation across all OSR settings.
- [ ] Step 2: In `analog_task.c`, create a FreeRTOS task that, once `tla2528_is_up()`, runs a fixed-period loop at `period_us = 1000000LL / (2 * cfg.analog_sample_rate_hz)` (twice the configured rate — one read per channel-slot, alternating, so each *channel* updates at `analog.sample_rate_hz`, matching decisions.md's corrected "how often firmware reads and converts" wording per channel). Maintain a local `bool next_is_ain5` toggle, flipped every read, seeded `false` (first conversion after `SEQ_START=1` is channel-ID-ascending, i.e., AIN4 first).
- [ ] Step 3: Each tick: call `tla2528_read_raw()`; on success, route the raw 16-bit value to channel 21 (`PressureRear`, AIN4) if `!next_is_ain5` or channel 20 (`PressureFront`, AIN5) if `next_is_ain5`, using the timestamp taken just before the read; flip `next_is_ain5`; on failure, log at a rate-limited level (not per-tick — this task runs at up to 1 kHz) and skip the record, per the existing sensor-failure-is-not-fatal policy (spec §4.5).
- [ ] Step 4: Registry: channel 20 = `PressureFront`, channel 21 = `PressureRear`, `data_type` = u16 (1), `sample_rate_hz = cfg.analog_sample_rate_hz`, `units = "raw"`, `scale = 1.0`, `offset = 0` — the transducer is undecided (owner scope note carried from rev1/rev2), so no physical-unit conversion happens on-device; the app converts later. This matches decisions.md P5's "store the chip's result, not app-side physical units" framing regardless of the OSR question below.
- [ ] Step 5: **OSR interaction (decisions.md P5):** the Step 1 decode already yields the correct 16-bit stored value at every OSR — averaged result at OSR>0, left-justified conversion at OSR=0. Store `*out_value` as-is; no per-OSR branching in the storage path.
- [ ] Step 6: **Channel-ID cross-check (now enabled — Figure 25 confirmed):** in the Step 3 routing, compare `*out_chan_id` against the expected sequencer position (4 when `!next_is_ain5`, 5 when `next_is_ain5`). On mismatch, diag-log loudly, drop the sample, and re-seed the toggle from the reported ID rather than trusting the local state — a silent front/rear pressure swap is the failure this guards against, and the ID field makes it detectable instead of invisible.
- [ ] Step 7: Flash with the analog front-end connected to a known DC source (or even just floating/AVDD-referenced for a coarse sanity check) and confirm via SD log that channel 20 and channel 21 both update at roughly `analog.sample_rate_hz`, and that toggling which physical channel has a distinguishable voltage lets you visually confirm front/rear routing is not swapped (this is the practical verification for Step 6's open item, given the bit-packing gap).
- [ ] Step 8: Commit.

---

### Task 24: Analog task scheduling + power-up + clock-stretch timeout insurance (rev2 C7, C8 — transcribed unchanged)

**Files:**
- Modify: `main/analog_task.c`
- Modify: `main/main.c` / boot sequencing (Task 17's location)

**Tier: cheap — doc/wiring only, values unaffected by the architecture correction.**

- [ ] Step 1: Confirm (doc comment, not new logic — the RC math already holds): the analog front end's TVS + 33Ω series + 100nF at the ADC pin gives τ=3.3µs, −3dB≈48kHz, settling in <20µs — irrelevant at ≤500Hz (the fastest valid `analog.sample_rate_hz`). This corrects spec §3.4's "No input protection" note, which does not describe IDL1 (documentation gap, resolved via decisions.md P4 as a doc-only fix in `idl0-app`, not firmware — do not attempt to edit the spec from here).
- [ ] Step 2: Only start `analog_task` after Task 17's boot-order wait **and** a successful `tla2528_init()` — if `tla2528_init()` returned `false` (Task 21 Step 2), do not create the analog task at all; leave analog channels absent from the registry for that boot rather than emitting a stream of read failures.
- [ ] Step 3: Confirm the IDF I2C transaction timeout set in Task 23 Step 1 (`≥10 ms`) is comfortably above the worst-case stretch this config can produce: at `OSC_SEL=0`/`CLK_DIV=0000b` (Task 21 Step 4, load-bearing) even `OSR=128` stretches ~128 µs — two orders of magnitude below the 10 ms timeout, so the margin is real, not nominal.
- [ ] Step 4: Add one comment tying this back to the shared-bus budget: analog reads share the same I2C bus as both pod IMUs' FIFO drains (Task 16); at the default 100 Hz `analog.sample_rate_hz` (→200 reads/sec after Task 23's 2× scheduling), each read is a ~4–6 byte transaction — negligible next to the pod IMU FIFO burst traffic Task 16's diag log tracks, but worth keeping in the same mental model if `analog.sample_rate_hz` is ever raised toward its 500 Hz ceiling.
- [ ] Step 5: Flash, confirm no transaction-timeout errors appear in the log across a multi-minute run with `analog.osr` set to a non-default value (e.g. 32) to exercise a real, non-trivial clock-stretch window.
- [ ] Step 6: Commit.

---

## Self-review notes

- **Spec coverage:** every rev2 row (A1–A21, R1–R32, B1–B7, C1–C12, D1–D4) maps to a task above, except the three IDL0-firmware-only audit rulings (decisions.md A1–A10, P4), which are explicitly out of scope per this plan's Precondition. rev2's Part C is intentionally **not** transcribed row-for-row (C1, C3, C5, C12) because decisions.md's TLA2528 section was corrected after rev2 was written — Tasks 21–24 rebuild that part against the corrected architecture while keeping the untouched rows (C2's mechanism, C4, C6–C11) intact.
- **Placeholder scan:** the two genuinely unresolved items (netlist-sourced SPI/SD pins; TLA2528 Figure 25 bit-packing) are called out as Open Questions with a concrete, safe fallback behavior specified in-task (an `#error` guard for the pins; an unmasked-raw-word storage plus deterministic-order routing for the analog read) — neither task is left as a vague TODO.
- **Type/signature consistency:** `imu_chip_drain()`'s `out_hg`/`max_hg`/`n_hg` parameters are declared in Task 4 and filled in by Task 9 without changing shape; `idl0_channel_registry_entry_t`, `idl0_format_channel_sample()`, and `digital_channel_cfg_t` are all used consistently with their existing (Task-0-context) signatures across Tasks 9, 10, 18, 19, 23.

---

**Task count: 24 total — 17 cheap-tier, 7 capable-tier** (Tasks 4, 7, 9, 12, 19, 22, 23 are capable-tier; all others are cheap-tier).
