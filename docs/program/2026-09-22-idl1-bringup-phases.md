# IDL1 firmware — bring-up phases

Written 2026-09-22, the day IDL1 boards arrived. Authority: `decisions.md`
(F1–F5 added today). This file re-slices `pod-firmware-plan.md` (24 tasks) into
the owner's three phases and records the corrections made while reviewing it.
Task numbers `PT<n>` refer to `pod-firmware-plan.md`.

Repo: `saucyeng/idl1-firmware` (private), forked from `idl0-firmware@f4f26c6`.
Bring-up happens on a different machine from the one that writes the code, so
the repo carries its own copy of the program docs under `docs/program/`.

---

## Corrections to the source plans (review findings)

| # | Where | Problem | Resolution |
|---|---|---|---|
| R1 | pod plan PT1, PT2 | Kconfig board choice + dual IMU components | Dropped by F2. No `CONFIG_IDL_BOARD_*`, DSO32 component deleted in phase 2. |
| R2 | pins spec §3, TLA block comment | Still says "autonomous mode… results read from RECENT_CHx" | Wrong per decisions P5 correction. Comment rewritten: auto-sequence, host-initiated bare reads. |
| R3 | pod plan PT18, PT21, PT22 | Invents names that duplicate the pins spec: `IDL1_PIN_SPEED_*`, `IDL1_TLA2528_ADDR_EXPECTED`, `IDL1_TLA_CH_ANALOG_*_AIN*` | The pins spec names win everywhere: `IDL1_PIN_WHEEL_F/_R`, `IDL1_I2C_ADDR_TLA`, `IDL1_TLA_CH_ANA_F/_R`, `IDL1_TLA_CH_LED_RED/_GREEN`. |
| R4 | pins spec §2–§4 | Neutral `IDL_PIN_*` alias layer, `pins_idl0.h` | Dropped by F2. `pins.h` is the IDL1 map. |
| R5 | pod plan Global Constraints, PT16 | Onboard default 960 Hz, pods 480 Hz — needs a per-IMU rate, but config has one `imu.sample_rate_hz` and the v3 header carries one `imu_sample_rate_hz` | Phase 2 keeps **one shared rate, default 480 Hz** (bus-safe). Per-IMU rate is a format change; it waits for the A8 format work (F3). 960 stays a valid value to try. **Owner may override.** |
| R6 | decisions P2 ladder | 7.5 Hz is not representable in the integer config field or the u16 header | Phase 2 valid set is {15 … 7680}. 7.5 deferred with the format work. |
| R7 | pod plan PT17 vs IDL0 `main.c` | IDL0 drives the SPI CS lines high first thing in boot. On IDL1 every CS target sits on the switched rail, so driving CS high before `PWR_EN` back-powers the unpowered IMU/SD through their input clamps | Order is `PWR_EN` high → 50 ms → idle CS lines → buses. |
| R8 | decisions H2 "device-ID read" | The TLA2528 has no device-ID register (Table 8 ends at 0x12) | Probe = `SYSTEM_STATUS` single-register read; ACK plus the documented reserved-bit value counts as identity. Raw byte always logged. |
| R9 | audit T53 | Ring-buffer budget was sized for 3 × 833 Hz. Phase 3 high-g adds 3 × `CHANNEL_SAMPLE` per triplet per pod | Phase 3 must re-check the writer ring budget before shipping high-g defaults. |
| R10 | idl0-app firmware catalog | Pulls `saucyeng/idl0-firmware` releases → IDL0 image offered to IDL1 hardware | F5 OTA project-name guard, phase 1. |
| R11 | `session.c:153` registry scale; PORT-CHECKLIST R15/A20 | Scale is `range/32768` and the checklist claims that matches the datasheet. **It does not for gyro**: ST G_So is 8.75/17.5/35/70/140 mdps/LSB for 250…4000 dps, i.e. 1.1468 × range/32768. Every gyro channel logged by IDL0 reads 12.8 % low in the app. High-g ±320 g is 10.417 mg/LSB, not 320/32768. Accel low-g and high-g ±32…256 g are fine. | Phase 2: registry scale from a datasheet sensitivity table (verified against `lsm6dsv320x.md` p.12 in idl1-hardware references), not from `range/32768`. The owner decides on an idl0-firmware backport and a spec §5.2 correction. |

Still-open audit rows carried into this fork: **T38** (SD mutex `portMAX_DELAY`,
unblocked by A7) and **T41** (A5 zero-filled IMU record) → phase 2. **T40**
(IMU0 SPI shares with SD) → phase 2 bring-up check per A10. **T42** → with A8.
**T43** → omitted per A6.

---

## Phase 1 — power on

Goal: flash, and the serial log answers "is every chip alive?" in one block.
Nothing is expected to *log data* except GPS; IMUs are detected, not driven.

1. **Identity.** CMake `project(idl1_firmware)`, boot banner, README/CLAUDE.md
   rewritten for IDL1 (spec pointer unchanged; authority = `docs/program/`).
   BLE UUIDs, advertised name, "IDL0" magic, status-line format unchanged (F3).
2. **`main/pins.h`** = pins spec §3 minus the alias block, with R2 and R3 applied.
3. **Call-site port** to `IDL1_PIN_*`: `sd_logger.c` (CS 7), `imu_driver.c`
   (only IMU0 is SPI, CS 21), `gps_driver.c`, `led_status.c` (GPIO15 active-low),
   `main.c` CS pre-idle.
4. **`PWR_EN`** (PT17 with R7): first statement of `app_main`, output, high,
   never reverted; 50 ms settle before any bus or GPS init.
5. **`main/i2c_bus.c`** (PT12): shared bus owner, 400 kHz, internal pull-ups off.
   `imu_driver_i2c.c` uses it.
6. **`components/tla2528`** (PT21 steps 1–2 + PT22 GPO config, no sequencer yet):
   probe exactly `IDL1_I2C_ADDR_TLA`, loud failure, boot survives. Ch 6/7 as
   push-pull GPO; `tla2528_led_set()`. Green on when up.
7. **IMU detection**: the DSO32 drivers read WHO_AM_I first; on `0x73` they record
   "LSM6DSV320X present" and decline to init (no DSO32 register writes to a
   DSV320X). Onboard via SPI, pods at 0x6A / 0x6B.
8. **Boot self-test report** (`main/bringup.c`): one block on the console and one
   diag-log line: PWR_EN, SD (mounted, size), IMU0 / pod F / pod R WHO_AM_I,
   TLA2528 ack + `SYSTEM_STATUS`, battery volts (ADC1_CH4 × 1.5), USER_BTN level,
   GPS bytes seen within a few seconds.
9. **OTA guard** (F5).

Bring-up checklist for the owner is in the repo: `docs/bringup.md`.

## Phase 2 — port old features (idl0-app compatible)

Goal: the idl0 app connects, configures, starts/stops a session, downloads it,
and parses it: three IMUs at low-g/gyro, GPS, HRM, markers.

- LSM6DSV320X port: PT2 (vendor, pinned tag), PT3, PT4 (chip-ops module; drop
  the `hg_*` parameters' implementation but keep the signature), PT5, PT6
  (with R6), PT7 (mode mapping; interlock inert until phase 3), PT11, PT14,
  PT15, PT16 (with R5). Delete the DSO32 component and the TEMP I2C framing.
- Config defaults move to valid DSV320X values (accel 16 g, gyro 2000 dps,
  480 Hz). Values the idl0 app sends that are off-list (32 g, 104/833 Hz, 125 dps)
  are rejected per field with the prior value kept (decisions P2/P3). The idl0
  app keeps working; it just cannot set those values.
- LED roles (H1): green = power + wireless connection, red = recording, via
  `tla2528_led_set()`; onboard GPIO15 keeps the pre-I2C / fault role.
- Audit carry-overs: T38 (bounded SD mutex wait, fault flag per A7), T41
  (A5 zero-filled record).
- Bring-up checks for the owner: IMU FIFO overrun counters stay 0 at 480 Hz on
  both pods; try 960 Hz by config push; IMU0/SD SPI coexistence (A10).

## Phase 3 — new features

- High-g: PT8, PT9, PT10, PT20 (hg fields), PT7 interlock live. Registry 24–29.
  Re-check the ring budget first (R9).
- Analog: PT20 (`analog.osr`), PT21 steps 3–7 (sequencer), PT23, PT24. Registry
  20/21 via TLA AIN5/AIN4.
- Wheel speed: PT18 (with R3 names), PT19. Registry 18/19.
- Battery voltage and USER_BTN: the report exists from phase 1. Owner to decide
  the BLE status line for battery and what the button does (default candidate:
  a marker channel via `digital.channels[]`, GPIO6, active-low).
- Then: A8 + per-IMU rate + 7.5 Hz as one schema-v4 change, together with the
  idl1 app (mobile) — firmware and app move to the new format together.
