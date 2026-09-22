# Register delta — Identity + boot — LSM6DSO32 vs LSM6DSV320X

## Delta table

| Name | DSO32 | DSV320X | Status | Page cite |
|------|-------|---------|--------|-----------|
| WHO_AM_I (0Fh) address | 0Fh | 0Fh | same | DSO32 p.66 / DSV320X p.70 |
| WHO_AM_I value | 6Ch (01101100) | 73h (01110011) | changed | DSO32 p.66 / DSV320X p.70 |
| FUNC_CFG_ACCESS (01h) default | 00h | 00h | same | DSO32 p.60 / DSV320X p.68 |
| FUNC_CFG_ACCESS bit 7 | FUNC_CFG_ACCESS | EMB_FUNC_REG_ACCESS | same (renamed) | DSO32 p.60 / DSV320X p.68 |
| FUNC_CFG_ACCESS bit 6 | SHUB_REG_ACCESS | SHUB_REG_ACCESS | same | DSO32 p.60 / DSV320X p.68 |
| FUNC_CFG_ACCESS bit 4 | — | FSM_WR_CTRL_EN | new | DSV320X p.68 |
| FUNC_CFG_ACCESS bit 3 | — | SW_POR | new | DSV320X p.68 |
| FUNC_CFG_ACCESS bit 2 | — | IF2_RESET | new | DSV320X p.68 |
| FUNC_CFG_ACCESS bit 0 | — | OIS_CTRL_FROM_UI | new | DSV320X p.68 |
| PIN_CTRL (02h) default | 3Fh (00111111) | 23h (00100011) | changed | DSO32 p.61 / DSV320X p.68 |
| IF_CFG register | — | 03h | new | DSV320X p.68 |
| ODR_TRIG_CFG register | — | 06h | new | DSV320X p.68 |
| CTRL register naming | CTRL1_XL, CTRL2_G, CTRL3_C, CTRL4_C, ... | CTRL1, CTRL2, CTRL3, CTRL4, ... | changed (naming) | DSO32 p.66–69 / DSV320X p.70–74 |
| CTRL3_C / CTRL3 address | 12h | 12h | same | DSO32 p.69 / DSV320X p.72 |
| CTRL3_C / CTRL3 default | 04h (00000100) | 44h (01000100) | changed | DSO32 p.69 / DSV320X p.72 |
| CTRL3_C / CTRL3 BOOT bit | bit 7 | bit 7 | same | DSO32 p.69 / DSV320X p.72 |
| BOOT function | Reboots memory content | Reboots memory content | same | DSO32 p.69 / DSV320X p.72 |
| BOOT auto-clear | Yes (automatically cleared) | Yes (automatically cleared) | same | DSO32 p.69 / DSV320X p.72 |
| CTRL3_C / CTRL3 BDU bit | bit 6, default 0 | bit 6, default 1 | changed (default) | DSO32 p.69 / DSV320X p.72 |
| CTRL3_C / CTRL3 H_LACTIVE / reserved | bit 5 (H_LACTIVE) | bit 5 (reserved 0) | changed | DSO32 p.69 / DSV320X p.72 |
| CTRL3_C / CTRL3 PP_OD / reserved | bit 4 (PP_OD) | bit 4 (reserved 0) | changed | DSO32 p.69 / DSV320X p.72 |
| CTRL3_C / CTRL3 SIM / reserved | bit 3 (SIM) | bit 3 (reserved 0) | changed | DSO32 p.69 / DSV320X p.72 |
| CTRL3_C / CTRL3 IF_INC | bit 2, default 1 | bit 2, default 1 | same | DSO32 p.69 / DSV320X p.72 |
| CTRL3_C / CTRL3 SW_RESET bit | bit 0 | bit 0 | same | DSO32 p.69 / DSV320X p.72 |
| SW_RESET function (DSO32) | Reset device | — | — | DSO32 p.69 |
| SW_RESET function (DSV320X) | — | Resets all control registers to their default value | — | DSV320X p.72 |
| SW_RESET auto-clear | Yes (automatically cleared) | Yes (automatically cleared) | same | DSO32 p.69 / DSV320X p.72 |
| CTRL_STATUS / ALL_INT_SRC / STATUS_REG | ALL_INT_SRC at 1Ah | CTRL_STATUS at 1Ah (new); ALL_INT_SRC at 1Dh | changed (address shift) | DSO32 p.66 / DSV320X p.70 |
| CTRL6_C / CTRL6 default | 00h | 08h | changed | DSO32 p.69 / DSV320X p.72 |
| Turn-on time (Ton) gyroscope | 35 ms | 40 ms | changed | DSO32 p.28 / DSV320X p.28 |
| Temperature stabilization time | 500 μs | 500 μs | same | DSO32 p.29 / DSV320X p.29 |
| PIN_CTRL reset during SW_RESET | Not explicitly stated | Not reset during SW_RESET | — | DSV320X p.68 |
| OIS_CTRL_FROM_UI reset during SW_RESET | — | Reset by SW_RESET | — | DSV320X p.72 |

## Notes

- **WHO_AM_I identity values differ**: DSO32 returns 6Ch while DSV320X returns 73h. This is the primary device identifier and firmware must check for the correct value.

- **BOOT bit semantics**: Both devices' BOOT bit reloads memory content (factory calibration values stored in OTP) and automatically clears after completion. No explicit status bit or polling mechanism is documented; completion is presumed via the auto-clear behavior.

- **SW_RESET scope differs**: DSO32 datasheet describes it as "reset device" with limited detail. DSV320X explicitly states it "resets all control registers to their default value" but does NOT reset PIN_CTRL (02h) or IF_CFG (03h) registers, which differ from the primary reset behavior.

- **FUNC_CFG_ACCESS register expansion**: DSV320X adds three new bits (FSM_WR_CTRL_EN bit 4, SW_POR bit 3, IF2_RESET bit 2, OIS_CTRL_FROM_UI bit 0) for auxiliary interface and embedded function control not present in DSO32. The base bits (FUNC_CFG_ACCESS and SHUB_REG_ACCESS) remain at bits 7 and 6.

- **Control register renaming**: DSO32 uses suffixed names (CTRL1_XL, CTRL2_G, CTRL3_C, CTRL4_C, CTRL5_C, CTRL6_C, CTRL7_G, CTRL8_XL, CTRL9_XL, CTRL10_C). DSV320X simplifies to CTRL1–CTRL10. Addresses remain identical (10h–19h).

- **Register address shifts in status/output area**: DSV320X reorganizes output registers, introducing CTRL_STATUS (1Ah, new read-only status), FIFO_STATUS1 (1Bh, new), FIFO_STATUS2 (1Ch, new), shifting ALL_INT_SRC and STATUS_REG from 1Ah and 1Eh to 1Dh and 1Eh respectively.

- **BDU default changed**: CTRL3_C bit 6 (BDU) defaults to 0 in DSO32 (continuous update). CTRL3 bit 6 defaults to 1 in DSV320X (block data update enabled by default). Firmware initializing DSV320X may not need explicit BDU configuration if blocking is desired.

- **CTRL6_C / CTRL6 default**: DSO32 default is 00h; DSV320X default is 08h. Exact bit meaning requires detailed register inspection but indicates a default configuration difference (likely ODR or mode setting).

- **PIN_CTRL default changed**: From 3Fh to 23h, indicating different pull-up or pad configuration defaults between parts.

- **Turn-on time (Ton) specification**: DSO32 specifies 35 ms while DSV320X specifies 40 ms for gyroscope turn-on. Both occur on power-up or after BOOT/SW_RESET completion.

- **Auxiliary interface (IF2) in DSV320X**: DSV320X supports a secondary auxiliary SPI/I3C interface (IF2) for OIS (Output In Sync) chain access and configuration. DSO32 has no equivalent interface. Certain registers (marked "R (IF2)" in DSV320X register map) are readable via IF2; SW_RESET and IF2_RESET in FUNC_CFG_ACCESS control IF2 behavior.

## Could not cite

- Explicit timing for BOOT completion (both datasheets state BOOT bit auto-clears but provide no Tboot specification).
- Explicit timing for SW_RESET completion (both datasheets state SW_RESET bit auto-clears but provide no Tsw_reset specification).
- Recommended initialization sequence (neither datasheet provides explicit startup sequence; both refer to register defaults and note that factory-calibration-loaded registers should not be changed).
- Accelerometer turn-on time separately from gyroscope (both datasheets group turn-on time and associate it with gyroscope; accelerometer may have different timing but is not separately specified).

## Coverage

Compared primary UI (universal interface) registers in both datasheets: WHO_AM_I, FUNC_CFG_ACCESS, PIN_CTRL, CTRL1–CTRL10 (and DSO32 suffixed variants), electrical characteristics (Ton, temperature stabilization). Examined register-map summary tables, control register descriptions, and electrical/timing specifications sections. DSV320X auxiliary interface (IF2) control registers and OIS chain not included in this delta (beyond noting IF2_RESET and OIS_CTRL_FROM_UI in FUNC_CFG_ACCESS).
