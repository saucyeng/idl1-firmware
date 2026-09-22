# Register delta — SPI specifics — LSM6DSO32 vs LSM6DSV320X

## Delta table

| name | DSO32 | DSV320X | status | page cite |
|---|---|---|---|---|
| SPI max clock frequency | 10 MHz | 10 MHz | same | DSO32 p.30 / DSV320X p.15 |
| SPI mode support | Modes 0 and 3 | Modes 0 and 3 | same | DSO32 p.37 / DSV320X p.35 |
| SDI/SDO timing edge | Driven at falling edge SPC, sampled at rising edge | Driven at falling edge SPC, sampled at rising edge | same | DSO32 p.37 / DSV320X p.35 |
| SIM (3-wire/4-wire mode) register | CTRL3_C (12h), bit [1] | IF_CFG (03h), bit [1] | changed | DSO32 p.69 / DSV320X p.60 |
| SIM default value | 0 (4-wire) | 0 (4-wire) | same | DSO32 p.69 / DSV320X p.60 |
| IF_INC (auto-increment) register | CTRL3_C (12h), bit [3] | CTRL3 (12h), bit [4] | changed | DSO32 p.69 / DSV320X p.56 |
| IF_INC default value | 1 (enabled) | 1 (enabled) | same | DSO32 p.69 / DSV320X p.56 |
| I2C/I3C disable register | CTRL4_C (13h), I2C_disable | IF_CFG (03h), I2C_I3C_disable | changed | DSO32 p.70 / DSV320X p.60 |
| I2C disable default value | 0 (I2C enabled) | 0 (I2C and I3C enabled) | same | DSO32 p.70 / DSV320X p.60 |
| Read/write bit format | Bit 0 = RW (1=read), Bits 1-7 = address, Bits 8-15 = data | Bit 0 = RW (1=read), Bits 1-7 = address, Bits 8-15 = data | same | DSO32 p.37 / DSV320X p.35 |
| Multi-byte read auto-increment | Enabled by IF_INC bit; address increments each byte block (8 clocks) | Enabled by IF_INC bit; address increments each byte block (8 clocks) | same | DSO32 p.37 / DSV320X p.35 |
| CS setup time (mode 3) | 5 ns | 5 ns (at VDDIO 1.8V); 25 ns (at VDDIO 1.2V) | changed | DSO32 p.30 / DSV320X p.15–17 |
| CS setup time (mode 0) | Not specified separately | 20 ns (at VDDIO 1.8V); 50 ns (at VDDIO 1.2V) | new | — / DSV320X p.15–17 |
| CS hold time (mode 3) | 20 ns | 20 ns (at VDDIO 1.8V); 50 ns (at VDDIO 1.2V) | changed | DSO32 p.30 / DSV320X p.15–17 |
| CS hold time (mode 0) | Not specified separately | 20 ns (at VDDIO 1.8V); 50 ns (at VDDIO 1.2V) | new | — / DSV320X p.15–17 |
| SDI setup time | 5 ns | 5 ns (at VDDIO 1.8V); 15 ns (at VDDIO 1.2V) | changed | DSO32 p.30 / DSV320X p.15–17 |
| SDI hold time | 15 ns | 15 ns (at VDDIO 1.8V); 30 ns (at VDDIO 1.2V) | changed | DSO32 p.30 / DSV320X p.15–17 |
| SDO valid output time | 50 ns max | 15 ns typ, 25 ns max (at VDDIO 1.8V); 30 ns max (at VDDIO 1.2V) | changed | DSO32 p.30 / DSV320X p.15–17 |
| SDO disable time | 50 ns max | 50 ns max (both VDDIO levels) | same | DSO32 p.30 / DSV320X p.15–17 |
| SPI clock high/low time | Not specified | 45 ns each (both VDDIO levels) | new | — / DSV320X p.15–17 |
| SDO_PU_EN (pull-up on SDO) register | PIN_CTRL (02h), bit [1] | PIN_CTRL (02h), bit [1] | same | DSO32 p.60 / DSV320X p.59 |
| SDO_PU_EN default value | 0 (pull-up disconnected) | 0 (pull-up disconnected) | same | DSO32 p.60 / DSV320X p.59 |
| Vdd_IO (power supply for I/O) range | 1.62–3.6 V | 1.08–3.6 V | changed | DSO32 p.28 / DSV320X p.15 |
| SPI timing spec condition | Single table at nominal conditions | Two tables: one at VDDIO 1.8V, one at VDDIO 1.2V | changed | DSO32 p.30 / DSV320X p.15–17 |

## Notes

- **Register location changes**: The most significant differences are register movements: SIM moves from CTRL3_C (12h) to IF_CFG (03h), and I2C disable moves from CTRL4_C (13h) to IF_CFG (03h). IF_INC stays in a register at address 12h but changes name from CTRL3_C to CTRL3 and shifts bit position from [3] to [4].

- **SPI timing voltage dependency**: DSV320X provides separate timing specifications for VDDIO at 1.8 V and 1.2 V, reflecting the expanded I/O voltage range (down to 1.08 V min vs 1.62 V min for DSO32). This is critical for low-voltage operation: CS setup and hold times are significantly relaxed (e.g., CS setup 5 ns at 1.8 V but 25 ns at 1.2 V).

- **SDO output timing improved at nominal voltage**: At VDDIO 1.8 V, DSV320X specifies SDO valid time with a 15 ns typical value (DSO32 has only 50 ns max), indicating potential faster reads, but the 25 ns max at 1.8 V and 30 ns max at 1.2 V shows variable performance across voltage corners.

- **I2C/I3C disable consolidation**: DSV320X consolidates interface disable logic: the single I2C_I3C_disable bit in IF_CFG replaces DSO32's separate I2C_disable in CTRL4_C, and DSV320X does not have a separate CTRL4_C register at all (CTRL4 at 13h has different content).

- **No explicit SPI read/write timing restriction prose**: Neither datasheet specifies a "read after write" recovery time or similar SPI command sequencing constraint in the SPI section. This is identical between parts.

- **Pull-up configuration identical for SDO**: Both parts use PIN_CTRL (02h) bit [1] = SDO_PU_EN with identical default (0 = disconnected), so no firmware change needed for this parameter.

- **Multi-byte read protocol identical**: Both use IF_INC to auto-increment address on every 8-clock block; no semantic difference.

## Could not cite

None. All rows have page citations from both datasheets.

## Coverage

Compared SPI timing tables (Table 6 in DSO32 at p.30; Tables 6–7 in DSV320X at p.15–17), SPI bus interface descriptions (Section 5.1.2 in DSO32 at p.37; Section 5.1.3 in DSV320X at p.35), and register descriptions for SIM, IF_INC, I2C disable, and SDO_PU_EN (Tables 23–24, 49–52 in DSO32; Tables 29–32, 58–59 in DSV320X). Searched for and found: SPI modes (0 and 3 on both), max clock (10 MHz on both), 4-wire vs 3-wire selection (SIM), multi-byte auto-increment (IF_INC), read/write bit encoding, CS timing (tables match), SDI/SDO setup/hold/valid times, I2C/I3C disable, SDO pull-up configuration, and I/O voltage ranges. Neither datasheet explicitly specifies SPI clock phase (CPOL/CPHA) terminology; both describe the protocol as mode 0 and mode 3 compatible with standard SPI definitions.

## Orchestrator review (2026-08-31 21:52)

- Bit numbers in the rows `IF_INC (auto-increment) register` and `SIM (3-wire/4-wire mode) register` are wrong. Verified in `lsm6dsv320x.md` line 2028 (register-map bit table for CTRL3 12h): `BOOT | BDU | 0 | 0 | 0 | IF_INC | 0 | SW_RESET` → **IF_INC is bit 2** on DSV320X, and it is bit 2 on DSO32 (CTRL3_C: BOOT, BDU, H_LACTIVE, PP_OD, SIM, IF_INC, 0, SW_RESET → SIM bit 3). Use `regs-ctrl.md` for CTRL3 bit positions. The *register moves* in this file (SIM and I2C_disable → IF_CFG 03h) stand.
- Also verified `lsm6dsv320x.md` line 1744 (IF_CFG 03h bit table): `SDA_PU_EN | SHUB_PU_EN | ASF_CTRL | H_LACTIVE | PP_OD | SIM | 0 | I2C_I3C_disable` → **SIM = bit 2, I2C_I3C_disable = bit 0, PP_OD = bit 3, H_LACTIVE = bit 4** on DSV320X.
