# Register delta — Interrupts (INT1/INT2 routing, data-ready semantics) — LSM6DSO32 vs LSM6DSV320X

## Delta table

| name | DSO32 | DSV320X | status | page cite |
|------|-------|---------|--------|-----------|
| INT1_CTRL (0Dh) register address | 0Dh | 0Dh | same | DSO32 p.64 / DSV320X p.65 |
| INT1_CTRL bit: INT1_DEN_DRDY_flag | bit 7 (exists) | — (removed) | removed | DSO32 p.64 / DSV320X — |
| INT1_CTRL bit: INT1_CNT_BDR | bit 6 | bit 6 | same | DSO32 p.64 / DSV320X p.65 |
| INT1_CTRL bit: INT1_FIFO_FULL | bit 5 | bit 5 | same | DSO32 p.64 / DSV320X p.65 |
| INT1_CTRL bit: INT1_FIFO_OVR | bit 4 | bit 4 | same | DSO32 p.64 / DSV320X p.65 |
| INT1_CTRL bit: INT1_FIFO_TH | bit 3 | bit 3 | same | DSO32 p.64 / DSV320X p.65 |
| INT1_CTRL bit: INT1_BOOT | bit 2 (exists) | — (removed) | removed | DSO32 p.64 / DSV320X — |
| INT1_CTRL bit: INT1_DRDY_G | bit 1 | bit 1 | same | DSO32 p.64 / DSV320X p.65 |
| INT1_CTRL bit: INT1_DRDY_XL | bit 0 | bit 0 | same | DSO32 p.64 / DSV320X p.65 |
| INT2_CTRL (0Eh) register address | 0Eh | 0Eh | same | DSO32 p.65 / DSV320X p.66 |
| INT2_CTRL bit: INT2_CNT_BDR | bit 6 | bit 6 | same | DSO32 p.65 / DSV320X p.66 |
| INT2_CTRL bit: INT2_FIFO_FULL | bit 5 | bit 5 | same | DSO32 p.65 / DSV320X p.66 |
| INT2_CTRL bit: INT2_FIFO_OVR | bit 4 | bit 4 | same | DSO32 p.65 / DSV320X p.66 |
| INT2_CTRL bit: INT2_FIFO_TH | bit 3 | bit 3 | same | DSO32 p.65 / DSV320X p.66 |
| INT2_CTRL bit: INT2_DRDY_TEMP | bit 2 (exists) | — (removed) | removed | DSO32 p.65 / DSV320X — |
| INT2_CTRL bit: INT2_DRDY_G | bit 1 | bit 1 | same | DSO32 p.65 / DSV320X p.66 |
| INT2_CTRL bit: INT2_DRDY_XL | bit 0 | bit 0 | same | DSO32 p.65 / DSV320X p.66 |
| INT2_CTRL bit: INT2_EMB_FUNC_ENDOP | — (absent) | bit 7 | new | DSO32 — / DSV320X p.66 |
| INT2_CTRL bit: INT2_DRDY_G_EIS | — (absent) | bit 2 | new | DSO32 — / DSV320X p.66 |
| MD1_CFG (5Eh) register address | 5Eh | 5Eh | same | DSO32 p.91 / DSV320X p.104 |
| MD1_CFG bit: INT1_SLEEP_CHANGE | bit 7 | bit 7 | same | DSO32 p.91 / DSV320X p.104 |
| MD1_CFG bit: INT1_SINGLE_TAP | bit 6 | bit 6 | same | DSO32 p.91 / DSV320X p.104 |
| MD1_CFG bit: INT1_WU | bit 5 | bit 5 | same | DSO32 p.91 / DSV320X p.104 |
| MD1_CFG bit: INT1_FF | bit 4 | bit 4 | same | DSO32 p.91 / DSV320X p.104 |
| MD1_CFG bit: INT1_DOUBLE_TAP | bit 3 | bit 3 | same | DSO32 p.91 / DSV320X p.104 |
| MD1_CFG bit: INT1_6D | bit 2 | bit 2 | same | DSO32 p.91 / DSV320X p.104 |
| MD1_CFG bit: INT1_EMB_FUNC | bit 1 | bit 1 | same | DSO32 p.91 / DSV320X p.104 |
| MD1_CFG bit: INT1_SHUB | bit 0 | bit 0 | same | DSO32 p.91 / DSV320X p.104 |
| MD2_CFG (5Fh) register address | 5Fh | 5Fh | same | DSO32 p.92 / DSV320X p.105 |
| MD2_CFG bit: INT2_SLEEP_CHANGE | bit 7 | bit 7 | same | DSO32 p.92 / DSV320X p.105 |
| MD2_CFG bit: INT2_SINGLE_TAP | bit 6 | bit 6 | same | DSO32 p.92 / DSV320X p.105 |
| MD2_CFG bit: INT2_WU | bit 5 | bit 5 | same | DSO32 p.92 / DSV320X p.105 |
| MD2_CFG bit: INT2_FF | bit 4 | bit 4 | same | DSO32 p.92 / DSV320X p.105 |
| MD2_CFG bit: INT2_DOUBLE_TAP | bit 3 | bit 3 | same | DSO32 p.92 / DSV320X p.105 |
| MD2_CFG bit: INT2_6D | bit 2 | bit 2 | same | DSO32 p.92 / DSV320X p.105 |
| MD2_CFG bit: INT2_EMB_FUNC | bit 1 | bit 1 | same | DSO32 p.92 / DSV320X p.105 |
| MD2_CFG bit: INT2_TIMESTAMP | bit 0 | bit 0 | same | DSO32 p.92 / DSV320X p.105 |
| STATUS_REG (1Eh) register address | 1Eh | 1Eh | same | DSO32 p.78 / DSV320X p.79 |
| STATUS_REG bit: TIMESTAMP_ENDCOUNT | — (absent) | bit 7 | new | DSO32 — / DSV320X p.79 |
| STATUS_REG bit: OIS_DRDY | — (absent) | bit 6 | new | DSO32 — / DSV320X p.79 |
| STATUS_REG bit: GDA_EIS | — (absent) | bit 5 | new | DSO32 — / DSV320X p.79 |
| STATUS_REG bit: XLHGDA (high-g accelerometer data-ready) | — (absent) | bit 4 | new | DSO32 — / DSV320X p.79 |
| STATUS_REG bit: TDA | bit 2 | bit 3 | changed | DSO32 p.78 / DSV320X p.79 |
| STATUS_REG bit: GDA | bit 1 | bit 2 | changed | DSO32 p.78 / DSV320X p.79 |
| STATUS_REG bit: XLDA | bit 0 | bit 1 | changed | DSO32 p.78 / DSV320X p.79 |
| Data-ready pulsed control: DSO32 location | COUNTER_BDR_REG1 (0Bh) bit 7 (dataready_pulsed, 75 μs pulse) | — (moved) | changed | DSO32 p.64 / — |
| Data-ready pulsed control: DSV320X location | — (removed from COUNTER_BDR_REG1) | CTRL4 (13h) bit 6 (DRDY_PULSED, 65 μs pulse) | changed | — / DSV320X p.68 |
| CTRL3_C (12h) bit: H_LACTIVE (active-low) | bit 5 | bit 5 | same | DSO32 p.77 / DSV320X p.67 |
| CTRL3_C (12h) bit: PP_OD (push-pull/open-drain) | bit 4 | bit 4 | same | DSO32 p.77 / DSV320X p.67 |
| CTRL4_C (13h) bit: INT2_ON_INT1 | bit 7 | bit 7 | same | DSO32 p.78 / DSV320X p.68 |
| CTRL7_G (16h) / CTRL7 (16h) — gyroscope vs high-g interrupts | Gyroscope config (G_HM_MODE, HP_EN_G, HPM_G) | High-g interrupts (INT1_DRDY_XL_HG, INT2_DRDY_XL_HG, LPF1_G_EN) | changed | DSO32 p.84 / DSV320X p.71 |
| TAP_CFG0 (56h) register address | 56h | 56h | same | DSO32 p.90 / DSV320X p.99 |
| TAP_CFG0 bit: LIR (latched interrupt) | bit 0 | bit 0 | same | DSO32 p.90 / DSV320X p.99 |
| TAP_CFG0 bit: LOW_PASS_ON_6D | — (absent) | bit 7 | new | DSO32 — / DSV320X p.99 |
| TAP_CFG0 bit: HW_FUNC_MASK_XL_SETTL | — (absent) | bit 6 | new | DSO32 — / DSV320X p.99 |
| ALL_INT_SRC (1Ah/1Dh) register address | 1Ah | 1Dh | changed | DSO32 p.74 / DSV320X p.78 |
| ALL_INT_SRC bit: HG_IA (high-g interrupt status) | — (absent) | bit 5 | new | DSO32 — / DSV320X p.78 |
| WAKE_UP_SRC register address | 1Bh | 45h | changed | DSO32 p.75 / DSV320X p.88 |
| TAP_SRC register address | 1Ch | 46h | changed | DSO32 p.76 / DSV320X p.89 |
| D6D_SRC register address | 1Dh | 47h | changed | DSO32 p.77 / DSV320X p.90 |
| HG_WAKE_UP_SRC (4Ch) | — (absent) | 4Ch | new | DSO32 — / DSV320X p.92 |
| HG_WAKE_UP_SRC bits: HG_IA, HG_WU_IA, HG_X_WU, HG_Y_WU, HG_Z_WU, HG_SHOCK_CHANGE_IA, HG_SHOCK_STATE | — (absent) | all present | new | DSO32 — / DSV320X p.92 |
| CTRL7 (16h) bit: INT1_DRDY_XL_HG (high-g data-ready on INT1) | — (absent) | bit 7 | new | DSO32 — / DSV320X p.71 |
| CTRL7 (16h) bit: INT2_DRDY_XL_HG (high-g data-ready on INT2) | — (absent) | bit 6 | new | DSO32 — / DSV320X p.71 |
| HG_FUNCTIONS_ENABLE (52h) | — (absent) | 52h | new | DSO32 — / DSV320X p.92 |
| HG_FUNCTIONS_ENABLE bits: HG_INTERRUPTS_ENABLE, HG_WU_CHANGE_INT_SEL, INT2_HG_WU, INT1_HG_WU, HG_SHOCK_DUR | — (absent) | all present | new | DSO32 — / DSV320X p.92 |
| COUNTER_BDR_REG1 (0Bh) bit: TRIG_COUNTER_BDR | 1-bit (XL vs GYRO only) | 2-bit TRIG_COUNTER_BDR_[1:0] (00=XL, 01=GY, 10=GY_EIS, 11=HG) | changed | DSO32 p.64 / DSV320X p.64 |
| COUNTER_BDR_REG1 (0Bh) bit: XL_HG_BATCH_EN | — (absent) | present | new | DSO32 — / DSV320X p.64 |

## Notes

- **Data-ready pulsing moved**: DSO32 implements `dataready_pulsed` (75 μs pulse width) in COUNTER_BDR_REG1 (0Bh bit 7), while DSV320X relocates this to CTRL4 (13h bit 6 `DRDY_PULSED`) with shorter 65 μs pulse width; latched mode is still the default (0).
- **High-g accelerometer as major new feature**: DSV320X adds comprehensive high-g interrupt support via new registers and bits: HG_WAKE_UP_SRC (4Ch), HG_FUNCTIONS_ENABLE (52h), CTRL7 high-g interrupt bits, XLHGDA in STATUS_REG, and HG_IA in ALL_INT_SRC. DSO32 has no high-g support.
- **Interrupt source register address reorganization**: WAKE_UP_SRC, TAP_SRC, D6D_SRC moved from sequential addresses (1Bh, 1Ch, 1Dh on DSO32) to widely spaced addresses (45h, 46h, 47h on DSV320X); ALL_INT_SRC also moved from 1Ah to 1Dh.
- **STATUS_REG bit rearrangement**: DSV320X adds TIMESTAMP_ENDCOUNT, OIS_DRDY, GDA_EIS, and XLHGDA bits at bits 7–4; original TDA, GDA, XLDA shifted down by 1 bit (DSO32: bits 2,1,0 → DSV320X: bits 3,2,1).
- **INT1/INT2 control bits removed**: INT1_BOOT and INT1_DEN_DRDY_flag removed from INT1_CTRL; INT2_DRDY_TEMP removed from INT2_CTRL (moved to CTRL4 as INT2_DRDY_TEMP control).
- **INT2_CTRL enhanced for EIS and embedded functions**: DSV320X adds INT2_EMB_FUNC_ENDOP and INT2_DRDY_G_EIS bits to INT2_CTRL.
- **CTRL7 register repurposed**: On DSO32, CTRL7_G is gyroscope-specific (G_HM_MODE, HP_EN_G, HPM); on DSV320X, CTRL7 is reused for high-g interrupt routing (INT1_DRDY_XL_HG, INT2_DRDY_XL_HG) and LPF1_G_EN.
- **TAP_CFG0 enhanced masking**: DSV320X adds LOW_PASS_ON_6D and HW_FUNC_MASK_XL_SETTL bits for advanced interrupt masking during filter settling.
- **TRIG_COUNTER_BDR expanded**: DSO32 uses 1-bit TRIG_COUNTER_BDR (XL vs GYRO); DSV320X expands to 2-bit field to include EIS gyroscope and high-g accelerometer batch triggers.
- **INT2_ON_INT1 (CTRL4) semantics updated**: DSV320X clarifies that INT2_EMB_FUNC_ENDOP and INT2_DRDY_G_EIS can be routed to INT1 via this bit; INT2_TIMESTAMP and INT2_DRDY_TEMP also movable (via CTRL4).
- **FIFO_STATUS2 address changed**: DSO32 uses 3Bh; DSV320X uses 1Ch; both report COUNTER_BDR_IA flag.

## Could not cite

None; all registers and bits traced to specific pages in datasheets.

## Coverage

Compared full interrupt-routing control registers (INT1_CTRL, INT2_CTRL, MD1_CFG, MD2_CFG, TAP_CFG0), all interrupt source registers (ALL_INT_SRC, WAKE_UP_SRC, TAP_SRC, D6D_SRC, HG_WAKE_UP_SRC), STATUS_REG with all data-ready bits, interrupt electrical config (H_LACTIVE, PP_OD in CTRL3_C), data-ready semantics (pulsed vs latched in COUNTER_BDR_REG1 → CTRL4 relocation, pulse width change), and all DSV320X-only high-g accelerometer interrupt support. 

DSO32 register maps covered: tables 19–20 and sections 9.7–9.51.  
DSV320X register maps covered: main map (lines 1540–1650) and sections 9.9–9.70 (interrupt, control, high-g).  
FIFO_STATUS2 address mismatch noted but not a core interrupt configuration register. SFLP (quaternion/bias/gravity) and MLC interrupt integration in DSV320X identified but deferred to embedded-functions pass.

## Orchestrator review (2026-08-31 21:58)

- The three rows `STATUS_REG bit: TDA/GDA/XLDA ... changed` and the Notes bullet "STATUS_REG bit rearrangement" are **wrong**. Verified in `lsm6dsv320x.md` line 2320 (STATUS_REG 1Eh bit table): `TIMESTAMP_ENDCOUNT | 0 | OIS_DRDY | GDA_EIS | XLHGDA | TDA | GDA | XLDA` → bit 7 TIMESTAMP_ENDCOUNT, bit 6 = 0, bit 5 OIS_DRDY, bit 4 GDA_EIS, **bit 3 XLHGDA, bit 2 TDA, bit 1 GDA, bit 0 XLDA**. XLDA/GDA/TDA are at the SAME positions as DSO32 → status `same`. The new-bit rows are right but one position too high (XLHGDA is bit 3, GDA_EIS bit 4, OIS_DRDY bit 5).
- Pattern: this file (and regs-spi.md) counted a reserved `0` column as a named bit. Treat every bit *position* in the regs-*.md files as needing re-verification against the extraction's bit-table row; addresses, defaults and encodings have checked out.
