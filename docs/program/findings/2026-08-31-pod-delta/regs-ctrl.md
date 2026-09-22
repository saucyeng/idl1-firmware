# Register delta — CTRL registers, ODR, full-scale, power modes, BDU — LSM6DSO32 vs LSM6DSV320X

## Delta table

| name | DSO32 | DSV320X | status | page cite |
|------|-------|---------|--------|-----------|
| CTRL1_XL (10h) renamed to CTRL1 | 10h, default 00h | 10h, default 00h | changed | DSO32 p.67 / DSV320X p.67 |
| CTRL1_XL / CTRL1 bit-field layout | ODR_XL[3:0] (bits 7-4), FS_XL[1:0] (bits 3-2), LPF2_XL_EN (bit 1), 0 (bit 0) | 0 (bit 7, must be 0), OP_MODE_XL[2:0] (bits 6-4), ODR_XL[3:0] (bits 3-0) | changed | DSO32 p.67 / DSV320X p.67 |
| CTRL2_G (11h) renamed to CTRL2 | 11h, default 00h | 11h, default 00h | changed | DSO32 p.68 / DSV320X p.68 |
| CTRL2_G / CTRL2 bit-field layout | ODR_G[3:0] (bits 7-4), FS_G[1:0] (bits 3-2), FS_125 (bit 1), 0 (bit 0) | 0 (bit 7, must be 0), OP_MODE_G[2:0] (bits 6-4), ODR_G[3:0] (bits 3-0) | changed | DSO32 p.68 / DSV320X p.68 |
| CTRL3_C renamed to CTRL3 | 12h, default 04h | 12h, default 44h | changed | DSO32 p.69 / DSV320X p.69 |
| CTRL3_C / CTRL3 bit-field layout | BOOT (bit 7), BDU (bit 6, default 0), H_LACTIVE (bit 5), PP_OD (bit 4), SIM (bit 3), IF_INC (bit 2, default 1), 0 (bit 1), SW_RESET (bit 0) | BOOT (bit 7), BDU (bit 6, default 1), 0 (bits 5-3, must be 0), IF_INC (bit 2, default 1), 0 (bit 1), SW_RESET (bit 0) | changed | DSO32 p.69 / DSV320X p.69 |
| CTRL4_C / CTRL4 | 13h, default 00h; bits SLEEP_G, INT2_on_INT1, DRDY_MASK, I2C_disable, LPF1_SEL_G | 13h, default 00h; bits 0 (must be 0), INT2_on_INT1, DRDY_MASK, INT2_DRDY_TEMP, DRDY_PULSED, INT2_IN_LH, 0, 0 (must be 0) | changed | DSO32 p.70 / DSV320X p.70 |
| CTRL5_C / CTRL5 | 14h, default 00h; bits XL_ULP_EN, ROUNDING[1:0], ST[1:0]_G, ST[1:0]_XL | 14h, default 00h; bits IF2_TA0_PID, 0 (bits 5-2, must be 0), BUS_ACT_SEL[1:0], INT_EN_I3C | changed | DSO32 p.71 / DSV320X p.71 |
| CTRL6_C / CTRL6 | 15h, default 00h; bits TRIG_EN, LVL1_EN, LVL2_EN, XL_HM_MODE (bit 4), USR_OFF_W, FTYPE[2:0] | 15h, default 08h; bits 0 (must be 0), LPF1_G_BW[2:0], 1 (bit 3, must be 1), FS_G[2:0] (bits 2-0) | changed | DSO32 p.72 / DSV320X p.71 |
| CTRL7_G / CTRL7 | 16h, default 00h; bits G_HM_MODE (bit 7), HP_EN_G (bit 6), HPM[1:0]_G (bits 5-4), 0 (bits 3-1), USR_OFF_ON_OUT (bit 0) | 16h, default 00h; bits INT1_DRDY_XL_HG (bit 7), INT2_DRDY_XL_HG (bit 6), 0 (bits 5-1, must be 0), LPF1_G_EN (bit 0) | changed | DSO32 p.73 / DSV320X p.72 |
| CTRL8_XL / CTRL8 | 17h, default 00h; bits HPCF_XL[2:0], HP_REF_MODE_XL, FASTSETTL_MODE_XL, HP_SLOPE_XL_EN, 0, LOW_PASS_ON_6D | 17h, default 00h; bits HP_LPF2_XL_BW[2:0], 0 (bits 4-2, must be 0), FS_XL[1:0] | changed | DSO32 p.73 / DSV320X p.73 |
| CTRL9_XL / CTRL9 | 18h, default E0h; bits 0, HP_REF_MODE_XL, XL_FASTSETTL_MODE, HP_SLOPE_XL_EN, LPF2_XL_EN, 0, USR_OFF_W, USR_OFF_ON_OUT | 18h, default 00h; bits 0 (must be 0), HP_REF_MODE_XL, XL_FASTSETTL_MODE, HP_SLOPE_XL_EN, LPF2_XL_EN, 0 (must be 0), USR_OFF_W, USR_OFF_ON_OUT | changed | DSO32 p.76 / DSV320X p.74 |
| CTRL10_C / CTRL10 | 19h, default 00h | 19h, default 00h; bits 0 (must be 0), EMB_FUNC_DEBUG, 0 (bits 5-4, must be 0), ST_G[1:0], ST_XL[1:0] | same | DSO32 p.76 / DSV320X p.76 |
| CTRL_STATUS | — | 1Ah, default output (read-only status register) | new | — / DSV320X p.76 |
| CTRL2_XL_HG (high-g accel) | — | 4Dh, default 00h; bits HG_USR_OFF_ON_WU, XL_HG_ST[1:0] | new | — / DSV320X p.93 |
| CTRL1_XL_HG (high-g accel) | — | 4Eh, default 00h; bits XL_HG_REGOUT_EN, HG_USR_OFF_ON_OUT, ODR_XL_HG[2:0], FS_XL_HG[2:0] | new | — / DSV320X p.93 |
| Accelerometer ODR encoding (low-g) — CTRL1_XL / CTRL1 | ODR_XL[3:0]: 0000=power-down, 0001=1.6 Hz LP / 12.5 Hz HP, 0010=12.5 Hz LP / 12.5 Hz HP, 0011=26 Hz, 0100=52 Hz, 0101=104 Hz, 0110=208 Hz, 0111=416 Hz, 1000=833 Hz, 1001=1.66 kHz, 1010=3.33 kHz, 1011=6.66 kHz, 11xx=not allowed | ODR_XL[3:0]: 0000=power-down, 0001=1.875 Hz LP, 0010=7.5 Hz, 0011=15 Hz, 0100=30 Hz, 0101=60 Hz, 0110=120 Hz, 0111=240 Hz, 1000=480 Hz, 1001=960 Hz, 1010=1.92 kHz, 1011=3.84 kHz, 1100=7.68 kHz, others=reserved | changed | DSO32 p.67 / DSV320X p.67 |
| Gyroscope ODR encoding — CTRL2_G / CTRL2 | ODR_G[3:0]: 0000=power-down, 0001=12.5 Hz LP, 0010=26 Hz LP, 0011=52 Hz LP, 0100=104 Hz, 0101=208 Hz, 0110=416 Hz, 0111=833 Hz, 1000=1.66 kHz, 1001=3.33 kHz, 1010=6.66 kHz, 11xx=not available | ODR_G[3:0]: 0000=power-down, 0010=7.5 Hz, 0011=15 Hz, 0100=30 Hz, 0101=60 Hz, 0110=120 Hz, 0111=240 Hz, 1000=480 Hz, 1001=960 Hz, 1010=1.92 kHz, 1011=3.84 kHz, 1100=7.68 kHz, others=reserved | changed | DSO32 p.68 / DSV320X p.68 |
| Accelerometer full-scale (low-g) — FS_XL[1:0] in CTRL1_XL / CTRL8 | 00=±4 g (default), 01=±32 g, 10=±8 g, 11=±16 g (4 settings) | FS_XL[1:0]: 01=±4 g, 10=±8 g, 11=±16 g; inferred 00=±2 g or reserved (3 documented settings) | changed | DSO32 p.67 / DSV320X p.73 |
| Accelerometer full-scale (high-g) — new high-g path | — | FS_XL_HG[2:0]: 000=±32 g, 001=±64 g, 010=±128 g, 011=±256 g, 100=±320 g | new | — / DSV320X p.93 |
| Gyroscope full-scale — FS_G[1:0] in CTRL2_G / CTRL6 | FS[1:0]_G: 00=±250 dps, 01=±500 dps, 10=±1000 dps, 11=±2000 dps; FS_125: 0=use FS[1:0]_G, 1=force ±125 dps | FS_G[2:0]: 000=reserved (default), 001=±250 dps, 010=±500 dps, 011=±1000 dps, 100=±2000 dps, 101=±4000 dps | changed | DSO32 p.68 / DSV320X p.71 |
| Accelerometer sensitivity (low-g) — FS = ±4 g | 0.122 mg/LSB | 0.122 mg/LSB | same | DSO32 p.25 / DSV320X p.12 |
| Accelerometer sensitivity (low-g) — FS = ±8 g | 0.244 mg/LSB | 0.244 mg/LSB | same | DSO32 p.25 / DSV320X p.12 |
| Accelerometer sensitivity (low-g) — FS = ±16 g | 0.488 mg/LSB | 0.488 mg/LSB | same | DSO32 p.25 / DSV320X p.12 |
| Accelerometer sensitivity (low-g) — FS = ±32 g | 0.976 mg/LSB | 0.976 mg/LSB (low-g) / 0.976 mg/LSB (high-g) | same | DSO32 p.25 / DSV320X p.12 |
| Accelerometer sensitivity (low-g) — FS = ±2 g | — | 0.061 mg/LSB | new | — / DSV320X p.12 |
| Accelerometer sensitivity (high-g) — FS = ±64 g | — | 1.952 mg/LSB | new | — / DSV320X p.12 |
| Accelerometer sensitivity (high-g) — FS = ±128 g | — | 3.904 mg/LSB | new | — / DSV320X p.12 |
| Accelerometer sensitivity (high-g) — FS = ±256 g | — | 7.808 mg/LSB | new | — / DSV320X p.12 |
| Accelerometer sensitivity (high-g) — FS = ±320 g | — | 10.417 mg/LSB | new | — / DSV320X p.12 |
| Gyroscope sensitivity — FS = ±125 dps | 4.375 mdps/LSB | — (±125 dps no longer available) | removed | DSO32 p.25 / — |
| Gyroscope sensitivity — FS = ±250 dps | 8.75 mdps/LSB | 8.75 mdps/LSB | same | DSO32 p.25 / DSV320X p.12 |
| Gyroscope sensitivity — FS = ±500 dps | 17.50 mdps/LSB | 17.50 mdps/LSB | same | DSO32 p.25 / DSV320X p.12 |
| Gyroscope sensitivity — FS = ±1000 dps | 35 mdps/LSB | 35 mdps/LSB | same | DSO32 p.25 / DSV320X p.12 |
| Gyroscope sensitivity — FS = ±2000 dps | 70 mdps/LSB | 70 mdps/LSB | same | DSO32 p.25 / DSV320X p.12 |
| Gyroscope sensitivity — FS = ±4000 dps | — | 140 mdps/LSB | new | — / DSV320X p.12 |
| Power mode — accelerometer high-performance enable bit | XL_HM_MODE (bit 4 of CTRL6_C, 15h): 0=high-performance enabled (default), 1=high-performance disabled | OP_MODE_XL[2:0] (bits 6-4 of CTRL1, 10h): 000=high-performance (default), 001=high-accuracy ODR, 010=reserved, 011=ODR-triggered, 100=LPM1, 101=LPM2, 110=LPM3, 111=normal | changed | DSO32 p.72 / DSV320X p.67 |
| Power mode — gyroscope high-performance enable bit | G_HM_MODE (bit 7 of CTRL7_G, 16h): 0=high-performance enabled (default), 1=high-performance disabled | OP_MODE_G[2:0] (bits 6-4 of CTRL2, 11h): 000=high-performance (default), 001=high-accuracy ODR, 010=reserved, 011=ODR-triggered, 100=sleep, 101=low-power | changed | DSO32 p.73 / DSV320X p.68 |
| Block Data Update (BDU) — register and bit | CTRL3_C (12h), bit 6, default 0: 0=continuous update (default), 1=update only after LSB and MSB read | CTRL3 (12h), bit 6, default 1: 0=continuous update, 1=update only after LSB and MSB read | changed | DSO32 p.69 / DSV320X p.69 |
| Address auto-increment (IF_INC) — register and bit | CTRL3_C (12h), bit 2, default 1: 0=disabled, 1=enabled (default) for I²C/SPI | CTRL3 (12h), bit 2, default 1: 0=disabled, 1=enabled (default) for I²C/SPI/MIPI I3C | same | DSO32 p.69 / DSV320X p.69 |
| Temperature output registers | OUT_TEMP_L (20h), OUT_TEMP_H (21h) | OUT_TEMP_L (20h), OUT_TEMP_H (21h) | same | DSO32 p.69 / DSV320X p.69 |
| Gyroscope output registers (low-g accel UI chain) | OUTX_L_G (22h), OUTX_H_G (23h), OUTY_L_G (24h), OUTY_H_G (25h), OUTZ_L_G (26h), OUTZ_H_G (27h) | OUTX_L_G (22h), OUTX_H_G (23h), OUTY_L_G (24h), OUTY_H_G (25h), OUTZ_L_G (26h), OUTZ_H_G (27h) | same | DSO32 p.69 / DSV320X p.69 |
| Accelerometer output registers (low-g UI chain) | OUTX_L_A (28h), OUTX_H_A (29h), OUTY_L_A (2Ah), OUTY_H_A (2Bh), OUTZ_L_A (2Ch), OUTZ_H_A (2Dh) | OUTX_L_A (28h), OUTX_H_A (29h), OUTY_L_A (2Ah), OUTY_H_A (2Bh), OUTZ_L_A (2Ch), OUTZ_H_A (2Dh) | same | DSO32 p.69 / DSV320X p.69 |
| Accelerometer output registers (high-g OIS_HG path) | — | UI_OUTX_L_A_OIS_HG (34h), UI_OUTX_H_A_OIS_HG (35h), UI_OUTY_L_A_OIS_HG (36h), UI_OUTY_H_A_OIS_HG (37h), UI_OUTZ_L_A_OIS_HG (38h), UI_OUTZ_H_A_OIS_HG (39h) | new | — / DSV320X p.69 |
| High-g accelerometer ODR encoding — ODR_XL_HG[2:0] in CTRL1_XL_HG | — | 000=power-down (default), 001=N/A, 010=N/A, 011=480 Hz, 100=960 Hz, 101=1.92 kHz, 110=3.84 kHz, 111=7.68 kHz | new | — / DSV320X p.93 |

## Notes

- **Renamed registers with architectural shift**: DSO32 CTRL registers use explicit sensor designation (CTRL1_XL, CTRL2_G, etc.) and power mode is a single bit (XL_HM_MODE, G_HM_MODE). DSV320X simplifies register names to CTRL1–CTRL10 but introduces a 3-bit OP_MODE field, allowing selection among 8 operating modes (high-performance, high-accuracy ODR, ODR-triggered, low-power variants, sleep for gyro, normal). This is a semantic re-encoding of the same control function at different bit positions (CTRL1 bit 6-4 vs CTRL6_C bit 4 for accelerometer; CTRL2 bit 6-4 vs CTRL7_G bit 7 for gyroscope).

- **BDU default polarity flip**: DSO32 ships BDU at 0 (continuous update); DSV320X ships BDU at 1 (block until both bytes read). Firmware may rely on the old default and need explicit BDU writes to maintain update timing.

- **Accelerometer ODR table overhaul**: ODR encoding is completely re-mapped. DSO32 encodes power-down at 0000, with 1.6 Hz LP at 0001, then power-of-two-ish spacing (12.5, 26, 52, 104, 208 Hz, 416, 833, then kHz). DSV320X power-down at 0000, then finer low-end (1.875, 7.5, 15, 30, 60, 120, 240 Hz), then 480, 960, 1920, 3840, 7680 Hz. Firmware must reprogram ODR setpoint for any migrated use case.

- **Gyroscope FS expansion and ±125 dps removal**: DSO32 supports ±125 dps via dedicated FS_125 bit in CTRL2_G; DSV320X removes ±125 dps entirely and expands upper range to ±4000 dps. FS_G[2:0] in CTRL6 now uses 001–101 (001=±250, 101=±4000); 000 is reserved. If hardware is driven at ±125 dps, it will need a re-tune to one of the supported DSV320X ranges.

- **High-g accelerometer path is separate**: DSV320X has a dedicated dual-path architecture with independent UI (low-g) and high-g channels. ODR and FS for high-g are configured in CTRL1_XL_HG (4Eh), not CTRL1 (10h). High-g chain runs at 480 Hz–7.68 kHz with ±32–±320 g FS. Output data for high-g appears on registers 34h–39h (UI_OUTX_L_A_OIS_HG, etc.), not the standard accelerometer output registers (28h–2Dh). Firmware reading OUTX_L_A / OUTX_H_A will get low-g (±2–±16 g) data only; high-g data is siloed.

- **Accelerometer FS encoding change (low-g)**: DSO32 FS_XL encodes ±4 g as 00 (default), ±8 as 10, ±16 as 11, ±32 as 01. DSV320X CTRL8 FS_XL[1:0] shows 01=±4 g, 10=±8 g, 11=±16 g (no 00 documented; specification hints ±2 g is available but encodes 00 and is not tabulated in CTRL8 description). Default FS_XL appears to be 00 (or must be set). Check register reset value or application note for ±2 g handling.

- **CTRL6_C / CTRL6 re-purpose**: DSO32 CTRL6_C is a filter and trigger control register with XL_HM_MODE power bit. DSV320X CTRL6 is now the gyroscope full-scale and LPF1 bandwidth register; gyroscope high-performance mode moved to OP_MODE_G in CTRL2. Firmware must no longer write XL_HM_MODE to CTRL6 (address 15h in both) and must use OP_MODE_XL in CTRL1 instead.

- **CTRL7_G / CTRL7 re-purpose**: DSO32 CTRL7_G is the gyroscope power and high-pass control. DSV320X CTRL7 is now high-g accelerometer interrupt routing (INT1_DRDY_XL_HG, INT2_DRDY_XL_HG) and gyroscope LPF1 enable (LPF1_G_EN). Gyroscope high-performance control is in CTRL2 OP_MODE_G; high-pass filter is no longer available in this register.

- **Output register address collision avoided**: DSV320X allocates high-g output registers at 34h–39h, distinct from standard accelerometer (28h–2Dh) and gyroscope (22h–27h) outputs. However, OIS and EIS paths also have output registers in the 2Eh–33h range. Firmware must read from the correct channel's output bank based on the use case (UI low-g, OIS, EIS, or HG).

## Could not cite

- FS_XL[1:0] encoding for 00 (reserved or ±2 g) in CTRL8 description table — table incomplete in both extraction and datasheet; only 01, 10, 11 documented, but 00 appears in register reset value and is hinted in features section (±2–±16 g low-g accel).

## Coverage

Compared CTRL1_XL–CTRL10_C (DSO32) vs CTRL1–CTRL10 and CTRL2_XL_HG / CTRL1_XL_HG (DSV320X) register maps (all addresses 10h–19h, plus 4Dh–4Eh for high-g). Extracted bit-field layouts, power-mode encoding, ODR and full-scale tables for both accelerometer (low-g and high-g) and gyroscope from register descriptions and mechanical characteristics tables (DSO32 p.67–76, DSV320X p.67–76 and p.93). Included output data register addresses (OUTX_L_A, etc., and high-g variants). BDU and IF_INC register/bit positions and default values noted. Sensitivity (mg/LSB, mdps/LSB) sourced from mechanical characteristics (DSO32 p.25, DSV320X p.12). Did not examine: FIFO control, embedded function configuration, or auxiliary I2C/I3C interface registers outside the primary CTRL bank.
