# IDL0 -> IDL1 pin-level delta (Pass 2)

Run id: 2026-08-31-hw-delta. Source of truth: KiCad netlists only (Pass 1 markdown files, cross-checked
against the raw `.net` s-expression exports where Pass 1 was ambiguous or stubbed). `idl1-hardware/README.md`
was **not** read or used, per brief.

Inputs read:
- `nets-idl0.md` (from `IDL0-fresh.net`, 2026-08-31 export, cross-checked against the January
  `IDL0.net` secondary export — see Deliverable 3.14)
- `nets-idl1.md` (from `IDL1.net`, 2026-08-31 export) — sections 4j-4m were a stub; resolved below
  from sections 1-2/4f-4i of that same file plus the raw `IDL1.net` s-expressions (libpart `pins` blocks
  for U4/TLA2528 and the XIAO-ESP32-C6 symbol, and the `comp` block for J2)
- `nets-breakout.md` (from `Breakout.net`)
- `idl0-firmware/main/pins.h`
- TLA2528 datasheet extract at `idl1-hardware/references/extracted/tla2528.md` — read only to resolve
  the ADDR-pin address-selection question in call-out 7 / Deliverable 4; not used for anything else, and
  the result is marked PROVISIONAL because the exact wiring on IDL1 doesn't match a documented row.

**How signals were matched across boards:** by function/net topology (what device pin the conductor
reaches and what it does), not by GPIO number or net name — several GPIO numbers are reused for
completely different functions between IDL0 and IDL1 (e.g. GPIO0/1/2/4/6/21/22/23 all changed meaning;
see the "gotcha" callouts inline). Where a net was renamed between boards (e.g. `/GPS_TX` ->
`GPS_RX_TO_MCU_TX`) the match was made by tracing both ends to the same device pins (ESP32 UART pin <->
GNSS module UART pin), not by string similarity.

---

## Deliverable 1 — the delta table

`changed?` legend: `same` / `moved` (same function, different GPIO) / `removed` (IDL0 only) / `new`
(IDL1 only) / `re-architected` (function moved to a different peripheral/bus) / `polarity` (same pin,
inverted sense).

| signal | IDL0 net | IDL0 GPIO | IDL1 net | IDL1 GPIO | changed? | firmware impact |
|---|---|---|---|---|---|---|
| Status LED (custom-board net) | `/LED_STATUS` (nets-idl0.md §1, §5a) | GPIO9 | `/MCU/LED1_A` driven via `/MCU/LED1_DRV` (nets-idl1.md §2, §4a) | none — driven by U4 (TLA2528) channel AIN6/GPIO6 over I2C, not an ESP32 pin | re-architected | Drop the ESP32-GPIO LED driver entirely; add a TLA2528 GPIO-expander write (channel 6, GPO mode, active-high) to light D1. |
| Onboard XIAO user LED (current bring-up workaround) | n/a — internal to the XIAO module, not on the custom-board netlist | GPIO15 (per pins.h comment; active-low) | same silicon, still present on the same module | GPIO15, unchanged | same (pin unaffected by board redesign) | Firmware should stop using the onboard-LED workaround now that D1 has a real, non-mirrored net on IDL1; GPIO15 itself doesn't change, just becomes unnecessary. |
| Second LED (green, D2) | none | n/a | `/MCU/LED2_A` driven via `/MCU/LED2_DRV` (nets-idl1.md §2, §4a) | none — U4 channel AIN7/GPIO7 over I2C | new | Add a TLA2528 GPO write on channel 7. **Gotcha:** this channel's datasheet name is "GPIO7," which is TLA2528-internal numbering — it has nothing to do with ESP32 GPIO7 (which is SD_CS on IDL1). Name the macro to avoid the collision (see Deliverable 4). |
| Buttons | `/BUTTONS` (nets-idl0.md §1, §5b) | GPIO5 | `/MCU/USER_BTN` (nets-idl1.md §1, §4b) | GPIO6 | moved + re-architected | Replace the planned ADC resistor-ladder read with a plain digital `gpio_get_level()` on GPIO6, internal pull-up enabled (no external pull-up resistor exists on this net in the IDL1 netlist — see call-out 3). |
| SPI SCK | `/SPI_SCK` (nets-idl0.md §1) | GPIO19 | `SPI_SCK` (nets-idl1.md §1) | GPIO19 | same | No change. |
| SPI MOSI | `/MOSI` (nets-idl0.md §1) | GPIO18 | `MOSI` (nets-idl1.md §1) | GPIO18 | same | No change. |
| SPI MISO | `/MISO` (nets-idl0.md §1) | GPIO20 | `MISO` (nets-idl1.md §1) | GPIO20 | same | No change. |
| CS — microSD | `/SD_CS` (nets-idl0.md §1) | GPIO23 | `SD_CS` (nets-idl1.md §1, §4e) | GPIO7 | moved | `#define IDL1_PIN_CS_SD GPIO_NUM_7` (was 23; GPIO23 is now SCL on IDL1 — do not reuse it here). |
| CS — onboard IMU (IMU0) | `/IMU1_CS` (nets-idl0.md §1) | GPIO2 | `IMU1_CS` (nets-idl1.md §1, §4d) | GPIO21 | moved | `#define IDL1_PIN_CS_IMU0 GPIO_NUM_21` (was 2; GPIO2 is now PWR_EN on IDL1 — do not reuse it here). |
| CS — front pod IMU (IMU1) | `/IMU_CS_FRONT` (nets-idl0.md §1) | GPIO21 | none — function removed | n/a | removed / re-architected | Drop the macro and the SPI chip-select logic; the front-pod IMU is now on the shared I2C bus (SDA=GPIO22, SCL=GPIO23), addressed by its own SDO/SA0 strap (see call-out 6). |
| CS — rear pod IMU (IMU2) | `/IMU_CS_REAR` (nets-idl0.md §1) | GPIO22 | none — function removed | n/a | removed / re-architected | Same as above for the rear pod. |
| GPS TX (host -> module) | `/GPS_TX` (nets-idl0.md §1, §5c note) | GPIO16 | `GPS_RX_TO_MCU_TX` (nets-idl1.md §1, §4c) | GPIO16 | same | No change; net renamed only (GNSS part changed MAX-M10S -> SAM-M10Q, see call-out 4). |
| GPS RX (module -> host) | `/GPS_RX` (nets-idl0.md §1) | GPIO17 | `GPS_TX_TO_MCU_RX` (nets-idl1.md §1, §4c) | GPIO17 | same | No change. |
| Wheel speed, front | `/SPEED_FRONT` (nets-idl0.md §1, §5h) | GPIO4 | `/MCU/WHEEL_F` (nets-idl1.md §1) | GPIO0 | moved + re-architected | `#define IDL1_PIN_WHEEL_F GPIO_NUM_0`. New RC front-end: R14 (4k7, pull-up to +3V3) + C12 (10nF to GND) ahead of the pin (nets-idl1.md §3) — no longer a bare open-drain edge input like IDL0. |
| Wheel speed, rear | `/SPEED_REAR` (nets-idl0.md §1, §5h) | GPIO6 | `/MCU/WHEEL_R` (nets-idl1.md §1) | GPIO1 | moved + re-architected | `#define IDL1_PIN_WHEEL_R GPIO_NUM_1`. Same RC front-end: R15 (4k7) + C15 (10nF) (nets-idl1.md §3). |
| Pressure / analog, front | `/PRESSURE_FRONT` (nets-idl0.md §1) | GPIO0 (ADC1_CH0) | `/MCU/ANA_F` -> U4 channel AIN5/GPIO5 (nets-idl1.md §2, §4f) | none — I2C only | re-architected | Replace `adc1_get_raw(ADC1_CHANNEL_0)` with an I2C read of TLA2528 channel 5. |
| Pressure / analog, rear | `/PRESSURE_REAR` (nets-idl0.md §1) | GPIO1 (ADC1_CH1) | `/MCU/ANA_R` -> U4 channel AIN4/GPIO4 (nets-idl1.md §2, §4f) | none — I2C only | re-architected | Replace `adc1_get_raw(ADC1_CHANNEL_1)` with an I2C read of TLA2528 channel 4. |
| Aux analog channel 1 (mainboard header) | none | n/a | `/MCU/ANA_1` -> U4 channel AIN2/GPIO2, from mainboard `J2` pin 1 (nets-idl1.md §2-3) | none — I2C only | new | Optional: add a TLA2528 channel-2 read if this header is populated. |
| Aux analog channel 2 (mainboard header) | none | n/a | `/MCU/ANA_2` -> U4 channel AIN3/GPIO3, from mainboard `J2` pin 2 (nets-idl1.md §2-3) | none — I2C only | new | Optional: add a TLA2528 channel-3 read. |
| I2C SDA | none in hardware — `IDL0_PIN_I2C_SDA` was a TEMP software reuse of the CS_IMU2 pin, no real I2C circuit existed (pins.h comment) | (macro value GPIO22, no bus) | `SDA` (nets-idl1.md §1) | GPIO22 | new (as real hardware) | `#define IDL1_PIN_I2C_SDA GPIO_NUM_22`. Same number as the old TEMP macro **by coincidence only** — the old one had no pull-up/bus; this one has R8 (4k7 to +3V3, nets-idl1.md §2-3). |
| I2C SCL | TEMP software reuse of CS_IMU1 pin (pins.h comment) | (macro value GPIO21, no bus) | `SCL` (nets-idl1.md §1) | GPIO23 | new (as real hardware) | `#define IDL1_PIN_I2C_SCL GPIO_NUM_23`. **Does not match** the old TEMP macro's GPIO21 — GPIO21 is now CS_IMU0 on IDL1. Porting the old value forward would silently break both SPI and I2C. |
| PWR_EN | none | n/a | `/MCU/PWR_EN` (nets-idl1.md §1, §4i) | GPIO2 | new | `#define IDL1_PIN_PWR_EN GPIO_NUM_2`. Must be driven HIGH at boot and held (see call-out 12). |
| BAT_SENSE | none (only raw BAT+/BAT- pass-through pins on IDL0's `J3`) | n/a | `/MCU/BAT_SENSE` (nets-idl1.md §1, §4i) | GPIO4 (ADC1) | new | `#define IDL1_PIN_BAT_SENSE GPIO_NUM_4`. Divider R2=1M / R3=2M -> `Vbat = 1.5 * Vadc` (nets-idl1.md §3, §4i). |
| Qwiic 3V3 rail | none | n/a | `/MCU/QWIIC_3V3` (nets-idl1.md §2) | n/a — power rail, not a GPIO | new | No pin macro; note only. Fed from `+3V3` through F3 (100mA fuse) to `J6` pin 2 — see call-out 12. |

---

## Deliverable 2 — the pins.h macro walk

All 17 `IDL0_PIN_*` macros in `pins.h`, in file order. Every value was checked against `nets-idl0.md`
and, where relevant, the raw `IDL0-fresh.net`; **all 16 board-net macros match the IDL0 netlist exactly** —
no discrepancies found between pins.h and the netlist itself (see also Deliverable 3.14 for the
primary-vs-secondary netlist comparison). The 17th (`IDL0_PIN_LED`) references a pin that isn't on the
custom board's own netlist at all (see row 1).

| macro | IDL0 GPIO (pins.h) | matches IDL0 netlist? | IDL1 GPIO or "no equivalent" | verdict |
|---|---|---|---|---|
| `IDL0_PIN_LED` | GPIO15 | N/A — GPIO15 is internal to the XIAO module; it never appears in `nets-idl0.md`'s ESP32C6 connected-nets table because it isn't wired on the custom board at all | GPIO15 still physically exists (same module), but IDL1's intended status LED is D1, driven via TLA2528 ch6 (I2C), not any ESP32 pin | replace-with — drive D1 via the TLA2528 GPIO expander; the reason for the original workaround (mirrored `/LED_STATUS` footprint) is moot since IDL1's LED isn't an ESP32 GPIO at all |
| `IDL0_PIN_BUTTONS` | GPIO5 | yes (`/BUTTONS`=GPIO5, nets-idl0.md §1) | GPIO6 (`/MCU/USER_BTN`) | renumber (5->6), and change read method (see call-out 3) |
| `IDL0_PIN_SPI_SCK` | GPIO19 | yes | GPIO19 (`SPI_SCK`) | keep |
| `IDL0_PIN_SPI_MOSI` | GPIO18 | yes | GPIO18 (`MOSI`) | keep |
| `IDL0_PIN_SPI_MISO` | GPIO20 | yes | GPIO20 (`MISO`) | keep |
| `IDL0_PIN_CS_SD` | GPIO23 | yes | GPIO7 (`SD_CS`) | renumber (23->7) |
| `IDL0_PIN_CS_IMU0` | GPIO2 | yes (`/IMU1_CS`=GPIO2) | GPIO21 (`IMU1_CS`) | renumber (2->21) |
| `IDL0_PIN_CS_IMU1` | GPIO21 | yes (`/IMU_CS_FRONT`=GPIO21) | no equivalent — front pod IMU moved to I2C | delete (replace with an I2C pod-IMU driver, not a CS macro) |
| `IDL0_PIN_CS_IMU2` | GPIO22 | yes (`/IMU_CS_REAR`=GPIO22) | no equivalent — rear pod IMU moved to I2C | delete |
| `IDL0_PIN_GPS_TX` | GPIO16 | yes | GPIO16 (`GPS_RX_TO_MCU_TX`) | keep |
| `IDL0_PIN_GPS_RX` | GPIO17 | yes | GPIO17 (`GPS_TX_TO_MCU_RX`) | keep |
| `IDL0_PIN_SPEED_FRONT` | GPIO4 | yes | GPIO0 (`/MCU/WHEEL_F`) | renumber (4->0) |
| `IDL0_PIN_SPEED_REAR` | GPIO6 | yes | GPIO1 (`/MCU/WHEEL_R`) | renumber (6->1) |
| `IDL0_PIN_PRESSURE_FRONT` | GPIO0 | yes | no ESP32 equivalent — TLA2528 channel 5 over I2C | replace-with |
| `IDL0_PIN_PRESSURE_REAR` | GPIO1 | yes | no ESP32 equivalent — TLA2528 channel 4 over I2C | replace-with |
| `IDL0_PIN_I2C_SDA` (TEMP) | GPIO22 | yes, consistent with the documented TEMP reuse of `/IMU_CS_REAR`'s pin | GPIO22 (`SDA` — a real bus this time) | keep the number (coincidental match), but rewrite the comment: this is no longer a software-only reuse, it's the real hardware I2C SDA with a pull-up |
| `IDL0_PIN_I2C_SCL` (TEMP) | GPIO21 | yes, consistent with the documented TEMP reuse of `/IMU_CS_FRONT`'s pin | GPIO23 (`SCL`) — **not** GPIO21 | renumber (21->23) — this is the one macro where a naive "keep the number" port silently breaks, since GPIO21 means something completely different on IDL1 (CS_IMU0) |

---

## Deliverable 3 — explicit call-outs

### 1. Status LED

IDL1 has **two** LEDs (D1 red, D2 green), and **neither is driven by an ESP32 GPIO**.

From `nets-idl1.md` §2/§3/§4a and the raw `IDL1.net` (U4/TLA2528 libpart `pins` block):
- `U4.5` (TLA2528 pin 5, symbol pin name `AIN6/GPIO6`) drives net `/MCU/LED1_DRV` -> `R1` (330R) -> net
  `/MCU/LED1_A` -> `D1` pin 2. `D1` pin 1 -> `GND`.
- `U4.6` (pin 6, `AIN7/GPIO7`) drives net `/MCU/LED2_DRV` -> `R5` (330R) -> net `/MCU/LED2_A` -> `D2` pin 2.
  `D2` pin 1 -> `GND`.
- Both `/MCU/LED1_A` and `/MCU/LED2_A` are KiCad's auto/hierarchical net names built from each LED's
  **anode** pin (`D`'s KiCad LED symbol pin 2 is named `A`; this is confirmed on IDL0 too, where the
  same D1 anode produced the auto-generated net name `Net-(D1-A)` — see nets-idl0.md §2/§5a). So pin 2 =
  anode (toward the driver), pin 1 = cathode (to GND) on both boards.
- Because the anode faces the driver and the cathode sits on GND, **driving the control pin HIGH lights
  the LED**: both D1 and D2 are **active-high**, same polarity convention as IDL0's `/LED_STATUS`
  (GPIO9 -> R1 330R -> `Net-(D1-A)` anode -> GND, nets-idl0.md §1/§2/§5a — also active-high).

**Firmware assignment for IDL1:** there is no ESP32 GPIO for the status LED at all. D1/D2 must be driven
via I2C through the TLA2528 GPIO-expander's GPO function on channels 6 and 7 respectively (GPO mode,
active-high). This means status-LED output now depends on the I2C bus and TLA2528 being initialized —
it can't be lit before I2C bring-up the way a raw GPIO could.

**GPIO9 on IDL1:** entirely unused. The raw `IDL1.net` XIAO pin list shows pin 24 (`GPIO9/BOOT`) as
`unconnected-(ESP32C6-GPIO9{slash}BOOT-Pad24)` (nets-idl1.md §1). The LED function moved off ESP32 GPIO9
completely — it isn't "mirrored" or reworked, it's just gone from the ESP32 side, freeing this
BOOT-strapping pin. See call-out 13.

### 2. Second LED on GPIO7

Present, but **not on ESP32 GPIO7**. ESP32 GPIO7 on IDL1 is `SD_CS` (`nets-idl1.md` §1, §4e — `ESP32C6.17`
-> net `SD_CS` -> `J1.2`). The second LED (D2, green) is driven by TLA2528 channel `AIN7/GPIO7`
(`nets-idl1.md` §2/§4a/§4f, U4 pin 6) — a completely different chip's internal channel numbering that
happens to share the digit 7. **This is a real naming hazard**: a macro named something like
`IDL1_PIN_LED2 = GPIO_NUM_7` would silently and catastrophically alias onto the SD chip-select pin. The
pins_idl1.h skeleton below deliberately names the TLA2528 channels with a distinct prefix
(`IDL1_TLA2528_CH_*`, plain integers, not `GPIO_NUM_*`) to prevent this collision.

### 3. /BUTTONS on GPIO5 (IDL0) vs GPIO6 (IDL1)

On IDL1 there is still a single button net, `/MCU/USER_BTN` (`nets-idl1.md` §1, §4b), on **GPIO6**
(`ESP32C6.21`). It is **not** a resistor ladder. The only component on this net besides the ESP32 pin is
`SW1` (`SW_Panasonic_EVQP2`, a tactile switch): `SW1.1` = `/MCU/USER_BTN`, `SW1.2` and `SW1.3` = `GND`
(nets-idl1.md §3/§4b). No resistor appears anywhere on `/MCU/USER_BTN` in the component or non-XIAO net
tables — so this is a single SPST switch to GND with **no external pull-up in the netlist**. Firmware
must enable the ESP32's internal pull-up (`gpio_pullup_en`) and read it as a plain digital input
(active-low: pressed = LOW). This is a functional change from IDL0's design intent, where the pins.h
comment says `/BUTTONS` on GPIO5 was "likely a resistor-ladder bank read by ADC" (that comment itself is
a guess made from the IDL0 board alone, since IDL0's `/BUTTONS` net also shows no ladder resistors in
`nets-idl0.md` §1/§5b — just `ESP32C6.18` and `J2.12`, i.e. a single off-board conductor to whatever sits
at the far end of the pod harness, not resolvable from this netlist). On IDL1 the button is now **onboard**
(`SW1`), not carried out to a pod connector at all.

### 4. GNSS module

IDL1 uses **SAM-M10Q** (`U3`, part `SAM-M10Q-00B`, nets-idl1.md §3/§4c), not MAX-M10S (which is IDL0's
`U4`, nets-idl0.md §3). UART GPIOs are **unchanged**: GPIO16 = host TX -> module RX (net renamed
`/GPS_TX` on IDL0 to `GPS_RX_TO_MCU_TX` on IDL1, both landing on `ESP32C6.7`/D6), GPIO17 = module TX ->
host RX (`/GPS_RX` -> `GPS_TX_TO_MCU_RX`, both on `ESP32C6.8`/D7). No TX/RX swap.

EXTINT / RESET_N / TIMEPULSE / SAFEBOOT_N / module SCL / module SDA are all unconnected on IDL1
(`unconnected-(U3-EXTINT-Pad19)`, `-RESET_N-Pad18`, `-TIMEPULSE-Pad7`, `-SAFEBOOT_N-Pad8`, `-SCL-Pad12`,
`-SDA-Pad9`; nets-idl1.md §2/§4c) — same situation as IDL0, where the equivalent MAX-M10S pins
(`EXTINT`, `LNA_EN`, `SCL`, `SDA`, `TIMEPULSE`, `VCC_RF`, `VIO_SEL`, `RESET`, `SAFEBOOT`) were also all
unconnected (nets-idl0.md §2). No new GPIO wired to any of these control lines on either board.

Incidental: IDL0 needed an external antenna (`E1`, `ANT-GNSSCP-TH25L1`, feeding MAX-M10S `RF_IN` via
`Net-(U4-RF_IN)`, nets-idl0.md §2/§3). IDL1's component table has no antenna part at all — SAM-M10Q has
an integrated patch antenna, so this is a simplification, not a pin change.

### 5. Pod connector pinout vs Breakout J1/J2

**IDL1 mainboard M8s** (`J4` = rear, `J5` = front; both `CDM801-06A-MP-F810-67`, 6-pin, nets-idl1.md §2/§3/§4g):

| J4 (rear) pin | net | function | J5 (front) pin | net | function |
|---|---|---|---|---|---|
| 1 | GND | ground | 1 | GND | ground |
| 2 | +3V3 | switched 3V3 | 2 | +3V3 | switched 3V3 |
| 3 | `/MCU/SDA_FIELD` | I2C data | 3 | `/MCU/SDA_FIELD` | I2C data |
| 4 | `/MCU/SCL_FIELD` | I2C clock | 4 | `/MCU/SCL_FIELD` | I2C clock |
| 5 | `/MCU/WHEEL_R_FIELD` | rear wheel-speed in | 5 | `/MCU/WHEEL_F_FIELD` | front wheel-speed in |
| 6 | `/MCU/ANA_R_FIELD` | rear analog/pressure in | 6 | `/MCU/ANA_F_FIELD` | front analog/pressure in |

`/MCU/SDA_FIELD` and `/MCU/SCL_FIELD` are the *same* two nets on both J4 and J5 (and on the Qwiic
connector `J6`) — this is one shared, multi-drop I2C bus reaching both pods and the Qwiic port, not two
independent buses.

**Breakout (pod) connectors** (`nets-breakout.md` §"J1/J2 Connector Pinout"):

| J1 (6-way) pin | net | function | J2 (3-way) pin | net | function |
|---|---|---|---|---|---|
| 1 | GND | ground | 1 | GND | ground |
| 2 | +3V3 | 3V3 | 2 | +3V3 | 3V3 |
| 3 | `/SDA` | I2C data | 3 | `/ANA` | analog (same net as J1 pin 6) |
| 4 | `/SCL` | I2C clock | — | — | — |
| 5 | `/WHEEL` | wheel-speed out | — | — | — |
| 6 | `/ANA` | analog out | — | — | — |

**Correspondence by function:** despite the brief's assumption that the M8s and the pod headers differ
in pin count, **J4/J5 (6-pin M8) map 1:1 by function onto the pod's J1 (6-way)** — GND/+3V3/SDA/SCL/
WHEEL/ANA on both sides, pin-for-pin by function (not necessarily by physical position, which isn't
recoverable from a netlist). The pod's separate 3-way `J2` does **not** need its own mainboard
counterpart: its `/ANA` net (nets-breakout.md "All Nets" table, code 2) is the *same conductor* as J1
pin 6's `/ANA`, just broken out to a second, smaller connector (`JST_B3B-PH-K-S`) for wiring convenience
(e.g. a sensor with a different mating connector) — it is a parallel tap, not an independent signal. So
there is no "orphan" signal on either side: every M8 pin has a pod-side counterpart, and the pod's extra
J2 duplicates a signal already present on J1.

**IDL0 pod connector, for comparison:** a single connector, `J2` (`DTM15-12PA`, 12-pin, nets-idl0.md §1/§3),
carried *everything* combined — shared `/SPI_SCK`/`/MOSI`/`/MISO`, separate `/IMU_CS_FRONT` and
`/IMU_CS_REAR`, `/PRESSURE_FRONT`, `/PRESSURE_REAR`, `/SPEED_FRONT`, `/SPEED_REAR`, `/BUTTONS`, `+3V3`,
`GND` — 12 signals on one harness for both corners. IDL1 replaces this with two independent per-corner
6-pin M8 harnesses sharing a common I2C bus instead of individual SPI chip-selects, plus the button
moved onboard entirely (see call-out 3). This is the single biggest architectural change in the pod
interface between the two boards.

### 6. Pod IMU bus

Breakout: `IMU1` = `LSM6DSV320XTR` (nets-breakout.md, IMU Configuration Analysis section), wired
**I2C**: pin 14 (`SDA`) -> `J1.3`, pin 13 (`SCL`) -> `J1.4` (nets-breakout.md "IMU Configuration
Analysis" table). Pin 1 (`SDO/SA0`) is tied through `R1` (10K) to solder jumper `JP1`
(`Net-(IMU1-SDO{slash}TA0)`), i.e. the I2C address bit is board-strappable via that jumper, not fixed.
SPI pins (`SCX`/pin3, no `SDX` used as SPI — the part's SPI-only pins are simply unconnected: pin 3
`unconnected-(IMU1-SCX-Pad3)`; pin 2 `unconnected-(IMU1-SDX-Pad2)` per nets-breakout.md "No-Connect
Analysis" — actually SDX/pin2 is the shared SDA/SDI pin and is unconnected as a distinct net, consistent
with I2C mode where it's multiplexed onto the SDA pin, not a separate SPI data line).

IDL1 mainboard side: the pod connectors (`J4`/`J5`) carry `/MCU/SDA_FIELD`/`/MCU/SCL_FIELD` — **I2C**, not
SPI (no CS/SCK/MOSI/MISO lines reach `J4` or `J5` at all; nets-idl1.md §2/§4g). **The two sides agree**:
pod is wired I2C, mainboard pod connectors carry I2C.

Compare with `pins.h`'s `IDL0_PIN_I2C_SDA`/`IDL0_PIN_I2C_SCL` TEMP block: that block documents a
*different* external IMU (Adafruit breakout with an I2C-only level shifter) reusing IDL0's freed SPI CS
pins (GPIO22/GPIO21) purely in software — there was no real I2C circuit on the IDL0 PCB itself (no
netlist evidence of pull-ups or a bus on those nets in nets-idl0.md). IDL1's I2C bus is real hardware
(GPIO22/GPIO23, with actual pull-ups — see call-out 10) but happens to only coincide on the SDA pin
number (22); SCL does not match (21 vs 23). See Deliverable 1/2 for the exact numbers.

### 7. Analog inputs

IDL0 read pressure on GPIO0/GPIO1 (ESP32 `ADC1_CH0`/`CH1`, nets-idl0.md §1, pins.h comments). On IDL1,
analog inputs move to the **TLA2528 (U4)** 8-channel I2C ADC/GPIO expander. Channel map, derived from the
raw `IDL1.net` TLA2528 libpart `pins` block (pin-number -> symbol pin name) joined against the `comp`
node list in `nets-idl1.md` §2/§4f:

| U4 pin | TLA2528 channel name | net | function |
|---|---|---|---|
| 15 | AIN0/GPIO0 | `unconnected-(U4-AIN0{slash}GPIO0-Pad15)` | not populated |
| 16 | AIN1/GPIO1 | `unconnected-(U4-AIN1{slash}GPIO1-Pad16)` | not populated |
| 1 | AIN2/GPIO2 | `/MCU/ANA_1` | mainboard aux analog input 1 (`J2` pin 1) |
| 2 | AIN3/GPIO3 | `/MCU/ANA_2` | mainboard aux analog input 2 (`J2` pin 2) |
| 3 | AIN4/GPIO4 | `/MCU/ANA_R` | rear pod analog/pressure (replaces `IDL0_PIN_PRESSURE_REAR`) |
| 4 | AIN5/GPIO5 | `/MCU/ANA_F` | front pod analog/pressure (replaces `IDL0_PIN_PRESSURE_FRONT`) |
| 5 | AIN6/GPIO6 | `/MCU/LED1_DRV` | GPO driving D1 (not an analog input — see call-out 1) |
| 6 | AIN7/GPIO7 | `/MCU/LED2_DRV` | GPO driving D2 (not an analog input — see call-out 2) |

**I2C address:** `U4` pin 11 (`ADDR`) connects to `Net-(U4-ADDR)`, which is shorted straight to `GND` by
`NT1` (a 2-pad, 0 Ω net-tie component; nets-idl1.md §2/§3/§4f). Per the TLA2528 datasheet
(`idl1-hardware/references/extracted/tla2528.md`, "7.3.5 I2C Address Selector," Table 2), the address is
set by a *resistor* from `ADDR` to either `DECAP` or `GND`; documented rows are R1(ADDR-to-DECAP)
∈ {0, 11k, 33k, 100k} with R2 = DNP, or R2(ADDR-to-GND) ∈ {11k, 33k, 100k} with R1 = DNP, or both DNP
-> `0x10`. **A direct 0 Ω short from ADDR to GND is not itself a documented row** (the closest is R2 =
11 kΩ -> `0x11`). This is genuinely ambiguous from the netlist plus datasheet alone — flagged
PROVISIONAL in Deliverable 4; recommend an I2C bus scan (0x10-0x17) at bring-up to confirm.

### 8. Wheel speed

IDL0 used GPIO4/GPIO6 as raw edge-interrupt inputs (`/SPEED_FRONT`, `/SPEED_REAR`, nets-idl0.md §1/§5h).
IDL1 uses **GPIO0** (`/MCU/WHEEL_F`) and **GPIO1** (`/MCU/WHEEL_R`) (nets-idl1.md §1), each with RC
conditioning between the Hall sensor and the pin:
- Front: `R14` (4k7, pull-up from `+3V3` to `/MCU/WHEEL_F`) + `C12` (10nF, `/MCU/WHEEL_F` to GND) on the
  MCU side; `R12` (330R) in series out to `/MCU/WHEEL_F_FIELD` (the connector-side conductor to `J5` pin
  5) (nets-idl1.md §3).
- Rear: `R15` (4k7 pull-up) + `C15` (10nF) on the MCU side; `R16` (330R) series out to
  `/MCU/WHEEL_R_FIELD` (`J4` pin 5) (nets-idl1.md §3).

This is an open-drain Hall sensor (Breakout `U1`, `ATS601LSG`, output on the pod's `/WHEEL` net,
nets-breakout.md) now pulled up and low-pass filtered on the mainboard, rather than a bare GPIO edge
input — firmware's ISR/interrupt-timing assumptions should account for the ~few-hundred-kHz RC corner
this introduces (exact corner depends on sensor source impedance; the R-C values alone give a rough
330 µs–3.3 ns range depending which resistor dominates — not fully determinable from the netlist).

### 9. SPI bus and chip-selects

IDL1 SCK/MOSI/MISO are the **same GPIOs** as IDL0 — GPIO19/18/20 (`SPI_SCK`/`MOSI`/`MISO`, nets-idl1.md
§1) — unchanged from `/SPI_SCK`/`/MOSI`/`/MISO` on IDL0 (nets-idl0.md §1). Only two devices remain on
this bus: the onboard IMU (`IMU1`, part changed `LSM6DSO32TR` -> `LSM6DSV320XTR`, same SPI pin
connections: `IMU1_CS`, `SPI_SCK`, `MOSI`, `MISO`, nets-idl1.md §3/§4d) and the microSD card (`J1`, part
changed `104031-0811` -> `502570-0893`, same SPI-mode wiring: `MISO`/`MOSI`/`SPI_SCK`/`SD_CS`, `DAT1`/
`DAT2` unconnected — nets-idl1.md §3/§4e). **The SD card did not move to SDIO/SDMMC** — it is still
plain SPI-mode on both boards (no `CLK`/`CMD`/`DAT0..3` wiring exists; `DAT1` and `DAT2` are explicitly
unconnected on both IDL0 and IDL1, nets-idl0.md §2 and nets-idl1.md §2).

Every CS:
| device | IDL0 CS GPIO | IDL1 CS GPIO |
|---|---|---|
| SD | GPIO23 (`/SD_CS`) | GPIO7 (`SD_CS`) |
| onboard IMU (IMU0/IMU1) | GPIO2 (`/IMU1_CS`) | GPIO21 (`IMU1_CS`) |
| front pod IMU | GPIO21 (`/IMU_CS_FRONT`) | *removed — moved to I2C, see call-out 6* |
| rear pod IMU | GPIO22 (`/IMU_CS_REAR`) | *removed — moved to I2C, see call-out 6* |

### 10. I2C bus

`SDA` = GPIO22, `SCL` = GPIO23 (`ESP32C6.5`/`ESP32C6.6`, nets-idl1.md §1). Devices on the bus:
- `U4` (TLA2528) directly, on the MCU-side `SDA`/`SCL` nets (pins 14/13, nets-idl1.md §2/§4f).
- Pod IMUs (front + rear), via `/MCU/SDA_FIELD`/`/MCU/SCL_FIELD` on `J4`/`J5` (call-out 5/6).
- Qwiic connector `J6` (`SM04B-SRSS-TB`), also on `/MCU/SDA_FIELD`/`/MCU/SCL_FIELD` (nets-idl1.md §2/§3h).
- GNSS module `U3` is **not** on this bus — its own SCL/SDA pins are unconnected (call-out 4).

Pull-ups: `R8` (4k7, `+3V3` to `SDA`) and `R9` (4k7, `+3V3` to `SCL`) sit on the MCU side (nets-idl1.md
§2). `R10` (33R, `SDA` to `/MCU/SDA_FIELD`) and `R11` (33R, `SCL` to `/MCU/SCL_FIELD`) are small series
resistors at the connector boundary, not additional pull-ups — so there is one shared pull-up pair for
the whole bus (onboard TLA2528 + both pods + Qwiic), and the series resistors are for ESD/ringing
protection at the M8/JST boundary. Rail: pull-ups reference `+3V3` (the *switched* rail, gated by
`PWR_EN` — see call-out 12), not `+3V3_ALWAYS`. Qwiic's own power pin is further isolated behind fuse
`F3` onto `/MCU/QWIIC_3V3` (call-out 12), but Qwiic's SDA/SCL are the same conductors as everything else
on the bus, just fused separately for power only.

### 11. JTAG-alias pins GPIO4 / GPIO5 / GPIO6 (MTMS / MTDO / MTCK)

| pin | IDL0 use | IDL1 use |
|---|---|---|
| GPIO4 (MTMS) | `/SPEED_FRONT` (nets-idl0.md §1/§5h) | `/MCU/BAT_SENSE` (nets-idl1.md §1) — still in use, different function |
| GPIO5 (MTDO — symbol pin name `GPIO5/A5/MTDO`, raw `IDL1.net` XIAO libpart) | `/BUTTONS` (nets-idl0.md §1/§5h) | **unconnected** — `unconnected-(ESP32C6-GPIO5{slash}A5{slash}MTDO-Pad18)` (nets-idl1.md §1) |
| GPIO6 (MTCK) | `/SPEED_REAR` (nets-idl0.md §1/§5h) | `/MCU/USER_BTN` (nets-idl1.md §1) — still in use, different function |

**IDL1 frees GPIO5** (the MTDO alias) — it is the only one of the three JTAG-alias pins left fully
unconnected on IDL1. GPIO4 and GPIO6 both remain committed to functional signals (just different ones
than on IDL0), so on-chip JTAG is still unusable with all three claimed unless GPIO5 alone is enough for
partial debug (it typically isn't — JTAG needs all four TMS/TDO/TCK/TDI-equivalents). Note the raw
netlist's symbol also names ESP32 pin 17 `GPIO7/MTDO` (nets-idl1.md-equivalent raw pin list) — i.e. the
XIAO symbol used in this schematic labels *both* GPIO5 and GPIO7 as "MTDO," which is inconsistent with
itself (only one GPIO can be MTDO); this looks like a symbol-authoring artifact and is reported here
verbatim rather than resolved, since it doesn't affect any net's electrical connectivity.

### 12. Power control

New on IDL1 (none of this exists on IDL0):
- **`/MCU/PWR_EN`** (`nets-idl1.md` §1/§4i): driven by **GPIO2** (`ESP32C6.3`), goes to `U2` (`TPS22919DCKR`
  load switch) pin 3 (`ON`, an active-high, **non-latching** level input per the part's own description
  string in the raw netlist: "ON is a LEVEL input - not latching").
- **`U2` (TPS22919) wiring** (raw `IDL1.net` libpart pins: 1=`IN`, 2=`GND`, 3=`ON`, 5=`QOD`, 6=`OUT`,
  joined against nets-idl1.md §2/§4i): pin 1 (`IN`) = `+3V3_ALWAYS`; pins 5/6 (`QOD`/`OUT`) = `+3V3`. So
  **`+3V3_ALWAYS` is the unswitched input** (feeds the ESP32-C6's own VCC directly — `ESP32C6.12`/`.23` —
  so the MCU stays powered to control `PWR_EN`), and **`+3V3` is the switched output**, feeding
  everything else: GNSS (`U3`), onboard IMU (`IMU1`), TLA2528 (`U4`), the LED resistors, the SD card
  (`J1`), and both M8 pod connectors (`J4`/`J5`).
- **`/MCU/BAT_SENSE`** (GPIO4, ESP32 ADC1 — *not* a TLA2528 channel): divider `R2` (1MΩ, `/MCU/BAT_+` to
  `/MCU/BAT_SENSE`) / `R3` (2MΩ, `/MCU/BAT_SENSE` to GND), filtered by `C5` (100nF) (nets-idl1.md §2/§3).
  Ratio = R3/(R2+R3) = 2/3, so `Vbat = 1.5 * Vadc`.
- **`+3V3_ALWAYS` vs `+3V3`**: covered above — `+3V3_ALWAYS` is unswitched/always-on (ESP32 only), `+3V3`
  is switched by `PWR_EN` (everything else).
- **`/MCU/QWIIC_3V3`**: `+3V3` through `F3` (100mA fuse) to `J6` pin 2 (nets-idl1.md §2/§3h) — so the
  Qwiic port's power is a fused branch of the *switched* rail, and also depends on `PWR_EN`.

**What firmware must do at boot:** because the TPS22919 `ON` pin is explicitly documented as a
non-latching level input, firmware must configure **GPIO2 as output and drive it HIGH early in boot**,
and keep it configured that way for the whole session — if GPIO2 is ever left as a floating/default
input, or reset to input state, the entire switched `+3V3` rail drops immediately, taking down GNSS,
the onboard IMU, the TLA2528 (and therefore the analog inputs and both status LEDs), the SD card, and
both pod connectors, while the ESP32 itself stays powered from `+3V3_ALWAYS`. GPIO2 is not an ESP32-C6
strapping pin, so it has no special reset-time behavior to account for beyond the usual GPIO defaults.

### 13. Strapping pins

ESP32-C6 strapping pins: GPIO8, GPIO9, GPIO15 (chip-level), and GPIO4/GPIO5 (JTAG source select).

| pin | exposed on this XIAO module's schematic symbol? | IDL0 connection | IDL1 connection | boot-mode risk |
|---|---|---|---|---|
| GPIO8 | No — does not appear in the 24-pin XIAO-ESP32C6-SMD symbol's pin list in either netlist (raw `IDL1.net` libpart `pins` block: only GPIO0,1,2,4,5,6,7,9,16,17,18,19,20,21,22,23 are broken out) | not assessable from this netlist (module-internal) | not assessable from this netlist (module-internal) | none from board wiring — the module hides this pin |
| GPIO9 (BOOT) | Yes, pin 24, symbol name `GPIO9/BOOT` | `/LED_STATUS` — R1 (330R) in series with D1 to GND (nets-idl0.md §1/§2/§5a). A 330Ω-plus-LED load to GND during the BOOT strap sample window is a comparatively light load against the internal pull-up, but it is a real DC path to GND that didn't exist as "nothing" — worth flagging as a plausible reason bring-up avoided this net (consistent with the pins.h comment about the footprint being "mirrored" and unusable, though the netlist alone can't confirm *why* it was avoided) | **Unconnected** — `unconnected-(ESP32C6-GPIO9{slash}BOOT-Pad24)` (nets-idl1.md §1) | IDL0: possible marginal loading during strap sampling; IDL1: none — pin is fully free |
| GPIO15 | No — not in the 24-pin symbol's pin list; it's the module's own onboard LED, internal to the XIAO, never reaches this board's schematic | n/a | n/a | none from board wiring — module-internal |
| GPIO4 (JTAG select / MTMS) | Yes | `/SPEED_FRONT` | `/MCU/BAT_SENSE` | committed to a function on both boards (see call-out 11) |
| GPIO5 (JTAG select / MTDO) | Yes | `/BUTTONS` | unconnected | freed on IDL1 (see call-out 11) |

### 14. IDL0 netlist staleness

Per `nets-idl0.md` §4 ("Staleness Comparison"): the January `IDL0.net` (SECONDARY, 54 nets, 15
components) differs from the fresh `IDL0-fresh.net` (PRIMARY, 42 nets, 13 components) by exactly 12
nets, and **every one of them** is an `unconnected-(IMU2-...)` or `unconnected-(IMU3-...)` no-connect pad
(INT1, INT2, NC, NC\_\_1, SCX, SDX for each of two extra IMU footprints). The 2 extra components in the
January file are those two IMU symbols (`IMU2`, `IMU3`) themselves. None of the 12 differing nets, nor
either extra component, touches any net that a `pins.h` macro references — every GPIO-to-net mapping
`pins.h` depends on (`/BUTTONS`, `/GPS_RX`, `/GPS_TX`, `/IMU1_CS`, `/IMU_CS_FRONT`, `/IMU_CS_REAR`,
`/LED_STATUS`, `/MISO`, `/MOSI`, `/PRESSURE_FRONT`, `/PRESSURE_REAR`, `/SD_CS`, `/SPEED_FRONT`,
`/SPEED_REAR`, `/SPI_SCK`) is present, identical, and connected to the identical `ESP32C6` pin number in
both files. **Conclusion: the two IDL0 netlists agree on everything `pins.h` needs; there is no
staleness-driven discrepancy to resolve.** The only real difference is that the January schematic still
had two extra (apparently unpopulated/unwired-beyond-NC) IMU footprints present that were later removed
from the design — a schematic cleanup with zero pin-map impact. `pins.h` agrees with both netlists on
every point of overlap.

---

## Deliverable 4 — proposed `pins_idl1.h` skeleton

Draft only — not written into the firmware repo, for human review.

```c
/* IDL1 firmware pin assignments — DRAFT, generated from netlist analysis.
 *
 * Derived from: nets-idl1.md / IDL1.net (KiCad export, 2026-08-31), cross-checked
 * against nets-breakout.md / Breakout.net for pod-side (front/rear IMU) agreement,
 * and against idl0-firmware/main/pins.h for the IDL0 -> IDL1 delta.
 *
 * NOT INSTALLED. This is a draft for human review before it becomes the real
 * pins.h for the IDL1 port.
 */

#pragma once

#include "driver/gpio.h"

/* ---- Power sequencing ---- */
#define IDL1_PIN_PWR_EN           GPIO_NUM_2   /* /MCU/PWR_EN -> U2 (TPS22919) ON pin.
                                                    Active-high, NON-LATCHING (datasheet string
                                                    on the part: "ON is a LEVEL input - not
                                                    latching") -- drive HIGH early in boot and
                                                    hold for the whole session, or the switched
                                                    +3V3 rail (everything except the MCU) drops. */
#define IDL1_PIN_BAT_SENSE        GPIO_NUM_4   /* /MCU/BAT_SENSE, ESP32 ADC1. Divider R2=1M /
                                                    R3=2M -> Vbat = 1.5 * Vadc. */

/* ---- Shared SPI bus: onboard IMU + microSD only. Front/rear pod IMUs moved to I2C. ---- */
#define IDL1_PIN_SPI_SCK          GPIO_NUM_19  /* SPI_SCK -- unchanged from IDL0 (was also 19) */
#define IDL1_PIN_SPI_MOSI         GPIO_NUM_18  /* MOSI -- unchanged from IDL0 (was also 18) */
#define IDL1_PIN_SPI_MISO         GPIO_NUM_20  /* MISO -- unchanged from IDL0 (was also 20) */
#define IDL1_PIN_CS_SD            GPIO_NUM_7   /* SD_CS (was GPIO23 on IDL0; GPIO23 is SCL now) */
#define IDL1_PIN_CS_IMU0          GPIO_NUM_21  /* IMU1_CS, onboard LSM6DSV320XTR
                                                    (was GPIO2 on IDL0; GPIO2 is PWR_EN now) */
/* CS_IMU1 / CS_IMU2 deleted: no equivalent on IDL1 -- front/rear pod IMUs are on the I2C bus. */

/* ---- GNSS UART (u-blox SAM-M10Q, was MAX-M10S on IDL0) ---- */
#define IDL1_PIN_GPS_TX           GPIO_NUM_16  /* GPS_RX_TO_MCU_TX: host TX -> module RX.
                                                    Unchanged GPIO from IDL0. */
#define IDL1_PIN_GPS_RX           GPIO_NUM_17  /* GPS_TX_TO_MCU_RX: module TX -> host RX.
                                                    Unchanged GPIO from IDL0. */
/* EXTINT / RESET_N / TIMEPULSE / SAFEBOOT_N / module SCL,SDA: all unconnected, same as IDL0. */

/* ---- Wheel speed: RC-filtered digital inputs (Hall sensor via pod, I2C-adjacent wiring) ---- */
#define IDL1_PIN_WHEEL_F          GPIO_NUM_0   /* /MCU/WHEEL_F. R14 (4k7 pull-up to +3V3) +
                                                    C12 (10nF to GND) ahead of the pin; R12 (330R)
                                                    series out to the pod (J5 pin 5). */
#define IDL1_PIN_WHEEL_R          GPIO_NUM_1   /* /MCU/WHEEL_R. R15 (4k7) + C15 (10nF); R16 (330R)
                                                    series out to the pod (J4 pin 5). */

/* ---- Button (onboard now, not carried out to the pod like IDL0's /BUTTONS) ---- */
#define IDL1_PIN_USER_BTN         GPIO_NUM_6   /* /MCU/USER_BTN, SW1 to GND. No external pull-up
                                                    resistor found on this net in the netlist --
                                                    enable the ESP32 internal pull-up. Active-low. */

/* ---- I2C bus: TLA2528 ADC/GPIO expander + both pod IMUs + Qwiic, all multi-drop ---- */
#define IDL1_PIN_I2C_SDA          GPIO_NUM_22  /* SDA. R8 (4k7) pull-up to +3V3.
                                                    Same GPIO NUMBER as IDL0's TEMP I2C_SDA macro,
                                                    but that was a software-only reuse of the
                                                    CS_IMU2 pin with no real bus -- coincidence,
                                                    not a carried-forward design. */
#define IDL1_PIN_I2C_SCL          GPIO_NUM_23  /* SCL. R9 (4k7) pull-up to +3V3.
                                                    DOES NOT match IDL0's TEMP I2C_SCL macro (21) --
                                                    GPIO21 is CS_IMU0 on IDL1. Do not port that value. */

/* TLA2528 (U4) channel map -- all reached over I2C, none are ESP32 GPIOs.
 * Plain integers on purpose (not GPIO_NUM_*) so they can never alias an ESP32 pin number --
 * see call-out 2: TLA2528 "GPIO6"/"GPIO7" channel names collide in digits only with ESP32
 * GPIO6 (USER_BTN) and GPIO7 (SD_CS); they are unrelated pins on a different chip. */
#define IDL1_TLA2528_CH_ANA_1     2   /* AIN2/GPIO2 <- /MCU/ANA_1, mainboard J2 pin 1 (aux input) */
#define IDL1_TLA2528_CH_ANA_2     3   /* AIN3/GPIO3 <- /MCU/ANA_2, mainboard J2 pin 2 (aux input) */
#define IDL1_TLA2528_CH_ANA_R     4   /* AIN4/GPIO4 <- /MCU/ANA_R, rear pod (J4 pin 6).
                                          Replaces IDL0_PIN_PRESSURE_REAR (was ESP32 GPIO1/ADC1_CH1). */
#define IDL1_TLA2528_CH_ANA_F     5   /* AIN5/GPIO5 <- /MCU/ANA_F, front pod (J5 pin 6).
                                          Replaces IDL0_PIN_PRESSURE_FRONT (was ESP32 GPIO0/ADC1_CH0). */
#define IDL1_TLA2528_CH_LED1      6   /* AIN6/GPIO6 -> /MCU/LED1_DRV -> R1 (330R) -> D1 (red) anode.
                                          GPO mode, active-high (drive HIGH to light). */
#define IDL1_TLA2528_CH_LED2      7   /* AIN7/GPIO7 -> /MCU/LED2_DRV -> R5 (330R) -> D2 (green) anode.
                                          GPO mode, active-high (drive HIGH to light). */
/* AIN0/GPIO0 (ch 0) and AIN1/GPIO1 (ch 1) are unpopulated -- U4 pads 15/16 unconnected. */

#define IDL1_TLA2528_ADDR         0x11  /* PROVISIONAL: ADDR pin is shorted directly to GND via
                                            0-ohm net-tie NT1. TI's datasheet Table 2 (7.3.5) only
                                            documents R1(ADDR->DECAP) or R2(ADDR->GND) at
                                            {11k,33k,100k} with the other leg DNP, or both legs DNP
                                            -> 0x10. A dead 0-ohm short on the R2-to-GND leg isn't
                                            an exact documented row -- 0x11 (the low end of the R2
                                            ladder) is the closest match but NOT confirmed. Verify
                                            with an I2C bus scan (0x10-0x17) at bring-up. */

/* ---- Pod bus (shared I2C; each pod's IMU sets its own address via an onboard strap) ---- */
/* Pod IMU (LSM6DSV320XTR, Breakout.net): SDA = J1 pin 3, SCL = J1 pin 4, confirmed I2C mode.
 * SDO/SA0 is tied through a 10K resistor to solder jumper JP1 -- the 7-bit address is
 * board-strapped per pod, not fixed. PROVISIONAL: exact address depends on JP1's populated
 * state, which isn't visible in a netlist -- read the jumper or scan the bus per pod at
 * bring-up. Needed so front and rear pods (same part, same bus) don't collide. */

/* ---- Power rails (informational only -- not GPIOs) ---- */
/* +3V3_ALWAYS : unswitched, feeds ESP32C6 VCC directly (U2 TPS22919 pin 1 = IN).
 *               MCU stays powered at all times to be able to drive PWR_EN.
 * +3V3        : switched output of U2 (pins 5/6 = QOD/OUT), gated by IDL1_PIN_PWR_EN.
 *               Feeds GNSS, onboard IMU, TLA2528, both LED resistors, SD card, both M8 pods.
 * /MCU/QWIIC_3V3 : +3V3 through F3 (100mA fuse) to Qwiic connector J6 pin 2.
 *                  Still depends on PWR_EN being asserted (it's downstream of the switch). */

/* ---- Removed from IDL0, no IDL1 equivalent ---- */
/* IDL0_PIN_CS_IMU1 (GPIO21) and IDL0_PIN_CS_IMU2 (GPIO22): front/rear pod IMUs are no longer
 * individually chip-selected over SPI -- see the I2C bus and pod-bus sections above. */
```
