# Register delta — FIFO — LSM6DSO32 vs LSM6DSV320X

## Delta table

| name | DSO32 | DSV320X | status | page cite |
|---|---|---|---|---|
| FIFO_CTRL1 (07h) address | 07h | 07h | same | DSO32 p.61 / DSV320X p.61 |
| FIFO_CTRL1 (07h) default | 00h | 00h | same | DSO32 p.61 / DSV320X p.61 |
| FIFO_CTRL1 (07h) WTM[7:0] | watermark bits [7:0] | watermark bits [7:0] | same | DSO32 p.61 / DSV320X p.61 |
| FIFO_CTRL2 (08h) address | 08h | 08h | same | DSO32 p.61 / DSV320X p.62 |
| FIFO_CTRL2 (08h) default | 00h | 00h | same | DSO32 p.61 / DSV320X p.62 |
| FIFO_CTRL2 (08h) bit 7 STOP_ON_WTM | STOP_ON_WTM | STOP_ON_WTM | same | DSO32 p.61 / DSV320X p.62 |
| FIFO_CTRL2 (08h) bit 6 FIFO_COMPR_RT_EN | FIFO_COMPR_RT_EN | FIFO_COMPR_RT_EN | same | DSO32 p.61 / DSV320X p.62 |
| FIFO_CTRL2 (08h) bit 5 | reserved (0) | reserved (0) | same | DSO32 p.61 / DSV320X p.62 |
| FIFO_CTRL2 (08h) bit 4 ODRCHG_EN / ODR_CHG_EN | ODRCHG_EN | ODR_CHG_EN | same | DSO32 p.61 / DSV320X p.62 |
| FIFO_CTRL2 (08h) bit 3 | reserved (0) | reserved (0) | same | DSO32 p.61 / DSV320X p.62 |
| FIFO_CTRL2 (08h) bits 2-1 UNCOMPR_RATE[1:0] | UNCOPTR_RATE[1:0] | UNCOMPR_RATE[1:0] | same | DSO32 p.61 / DSV320X p.62 |
| FIFO_CTRL2 (08h) bit 0 watermark extension | WTM8 (9-bit watermark) | reserved (0) (8-bit watermark only) | changed | DSO32 p.61 / DSV320X p.62 |
| FIFO watermark width | 9 bits (WTM[8:0] in CTRL1+CTRL2) | 8 bits (WTM[7:0] in CTRL1 only) | changed | DSO32 p.61 / DSV320X p.61 |
| FIFO_CTRL3 (09h) address | 09h | 09h | same | DSO32 p.62 / DSV320X p.62 |
| FIFO_CTRL3 (09h) default | 00h | 00h | same | DSO32 p.62 / DSV320X p.62 |
| FIFO_CTRL3 (09h) BDR_GY[3:0] rates | 0001:12.5Hz 0010:26Hz 0011:52Hz 0100:104Hz 0101:208Hz 0110:417Hz 0111:833Hz 1000:1.667kHz 1001:3.333kHz 1010:6.667kHz 1011:6.5Hz | 0001:1.875Hz 0010:7.5Hz 0011:15Hz 0100:30Hz 0101:60Hz 0110:120Hz 0111:240Hz 1000:480Hz 1001:960Hz 1010:1.92kHz 1011:3.84kHz 1100:7.68kHz | changed | DSO32 p.62 / DSV320X p.62 |
| FIFO_CTRL3 (09h) BDR_XL[3:0] rates | 0001:12.5Hz 0010:26Hz 0011:52Hz 0100:104Hz 0101:208Hz 0110:417Hz 0111:833Hz 1000:1.667kHz 1001:3.333kHz 1010:6.667kHz 1011:1.6Hz | 0001:1.875Hz 0010:7.5Hz 0011:15Hz 0100:30Hz 0101:60Hz 0110:120Hz 0111:240Hz 1000:480Hz 1001:960Hz 1010:1.92kHz 1011:3.84kHz 1100:7.68kHz | changed | DSO32 p.62 / DSV320X p.62 |
| FIFO_CTRL4 (0Ah) address | 0Ah | 0Ah | same | DSO32 p.63 / DSV320X p.63 |
| FIFO_CTRL4 (0Ah) default | 00h | 00h | same | DSO32 p.63 / DSV320X p.63 |
| FIFO_CTRL4 (0Ah) DEC_TS_BATCH[1:0] | decimation 1/8/32 | decimation 1/8/32 | same | DSO32 p.63 / DSV320X p.63 |
| FIFO_CTRL4 (0Ah) ODR_T_BATCH[1:0] temperature rates | 01:1.6Hz 10:12.5Hz 11:52Hz | 01:1.875Hz 10:15Hz 11:60Hz | changed | DSO32 p.63 / DSV320X p.63 |
| FIFO_CTRL4 (0Ah) bit 3 | reserved (0) | G_EIS_FIFO_EN: enable EIS gyro FIFO batching | new | — / DSV320X p.63 |
| FIFO_CTRL4 (0Ah) FIFO_MODE[2:0] | 000:Bypass 001:FIFO 010:Reserved 011:Continuous-to-FIFO 100:Bypass-to-Continuous 110:Continuous 111:Bypass-to-FIFO | 000:Bypass 001:FIFO 010:continuousWTM-to-full 011:Continuous-to-FIFO 100:Bypass-to-Continuous 110:Continuous 111:Bypass-to-FIFO | changed | DSO32 p.63 / DSV320X p.63 |
| COUNTER_BDR_REG1 (0Bh) address | 0Bh | 0Bh | same | DSO32 p.64 / DSV320X p.64 |
| COUNTER_BDR_REG1 (0Bh) default | 00h | 00h | same | DSO32 p.64 / DSV320X p.64 |
| COUNTER_BDR_REG1 (0Bh) bit 7 dataready_pulsed | dataready_pulsed | reserved (0) | changed | DSO32 p.64 / DSV320X p.64 |
| COUNTER_BDR_REG1 (0Bh) bits 6-5 TRIG_COUNTER_BDR | 1-bit: selects XL or GY | 2-bit: selects XL/GY/EIS_GY/HG | changed | DSO32 p.64 / DSV320X p.64 |
| COUNTER_BDR_REG1 (0Bh) TRIG_COUNTER_BDR field | 0:XL batch 1:GY batch | 00:low-g XL batch 01:GY batch 10:EIS GY batch 11:high-g XL batch | changed | DSO32 p.64 / DSV320X p.64 |
| COUNTER_BDR_REG1 (0Bh) bit 3 RST_COUNTER_BDR | RST_COUNTER_BDR reset bit | — removed | removed | DSO32 p.64 / — |
| COUNTER_BDR_REG1 (0Bh) bit 4 XL_HG_BATCH_EN | — (does not exist) | XL_HG_BATCH_EN: enable high-g accel FIFO batching | new | — / DSV320X p.64 |
| COUNTER_BDR_REG1 (0Bh) bits 1-0 CNT_BDR_TH[10:8] | 3-bit upper threshold (11-bit total with REG2) | 2-bit upper threshold (10-bit total with REG2) | changed | DSO32 p.64 / DSV320X p.64 |
| COUNTER_BDR_REG2 (0Ch) address | 0Ch | 0Ch | same | DSO32 p.64 / DSV320X p.64 |
| COUNTER_BDR_REG2 (0Ch) CNT_BDR_TH[7:0] | lower 8 bits of 11-bit threshold | lower 8 bits of 10-bit threshold | changed | DSO32 p.64 / DSV320X p.64 |
| FIFO_STATUS1 address | 3Ah | 1Bh | changed | DSO32 p.84 / DSV320X p.76 |
| FIFO_STATUS1 (3Ah/1Bh) DIFF_FIFO bits | DIFF_FIFO[7:0] (lower 8 of 10-bit count) | DIFF_FIFO[7:0] (full 8-bit count) | changed | DSO32 p.84 / DSV320X p.76 |
| FIFO_STATUS2 address | 3Bh | 1Ch | changed | DSO32 p.84 / DSV320X p.76 |
| FIFO_STATUS2 (3Bh/1Ch) DIFF_FIFO upper bits | DIFF_FIFO[9:8] in bits 1-0 | DIFF_FIFO_8 in bit 0 only | changed | DSO32 p.84 / DSV320X p.76 |
| FIFO_STATUS2 (3Bh/1Ch) other status flags | FIFO_WTM_IA OVR_IA FULL_IA COUNTER_BDR_IA OVR_LATCHED | FIFO_WTM_IA OVR_IA FULL_IA COUNTER_BDR_IA OVR_LATCHED (same) | same | DSO32 p.84 / DSV320X p.76 |
| FIFO_DATA_OUT_TAG (78h) address | 78h | 78h | same | DSO32 p.96 / DSV320X p.117 |
| FIFO_DATA_OUT_TAG (78h) bit 7 TAG_PARITY | TAG_PARITY (parity bit) | reserved (spare) | removed | DSO32 p.96 / DSV320X p.117 |
| FIFO_DATA_OUT_TAG (78h) TAG_SENSOR[4:0] | 5-bit sensor identifier | 5-bit sensor identifier | same | DSO32 p.96 / DSV320X p.117 |
| FIFO_DATA_OUT_TAG (78h) TAG_CNT[1:0] | 2-bit time slot counter | 2-bit time slot counter | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x00 | — (not in DSO32) | FIFO empty | new | — / DSV320X p.117 |
| FIFO tag 0x01 | Gyroscope NC | Gyroscope NC | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x02 | Accelerometer NC | Accelerometer NC | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x03 | Temperature | Temperature | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x04 | Timestamp | Timestamp | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x05 | CFG_Change | CFG_Change | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x06-0x0D | Accelerometer/Gyroscope compression tags (2xC, 3xC, NC_T_1/2) | Accelerometer/Gyroscope compression tags (2xC, 3xC, NC_T_1/2) | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x0E-0x11 | Sensor Hub Slave 0-3 | Sensor hub target 0-3 | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x12 | Step Counter | Step counter | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x13 | — (not in DSO32) | SFLP game rotation vector | new | — / DSV320X p.117 |
| FIFO tag 0x16 | — (not in DSO32) | SFLP gyroscope bias | new | — / DSV320X p.117 |
| FIFO tag 0x17 | — (not in DSO32) | SFLP gravity vector | new | — / DSV320X p.117 |
| FIFO tag 0x18 | — (not in DSO32) | High-g accelerometer peak value | new | — / DSV320X p.117 |
| FIFO tag 0x19 | Sensor Hub Nack | Sensor hub nack | same | DSO32 p.96 / DSV320X p.117 |
| FIFO tag 0x1A | — (not in DSO32) | MLC result | new | — / DSV320X p.117 |
| FIFO tag 0x1B | — (not in DSO32) | MLC filter | new | — / DSV320X p.117 |
| FIFO tag 0x1C | — (not in DSO32) | MLC feature | new | — / DSV320X p.117 |
| FIFO tag 0x1D | — (not in DSO32) | High-g accelerometer | new | — / DSV320X p.117 |
| FIFO tag 0x1E | — (not in DSO32) | Enhanced EIS gyroscope | new | — / DSV320X p.117 |
| FIFO tag 0x1F | — (not in DSO32) | FSM results | new | — / DSV320X p.117 |
| FIFO_DATA_OUT_X/Y/Z_L/H (79h-7Eh) addresses | 79h-7Eh | 79h-7Eh | same | DSO32 p.96 / DSV320X p.117 |
| FIFO_DATA_OUT_X/Y/Z_L/H (79h-7Eh) format | 1 TAG byte (78h) + 6 data bytes (79h-7Eh) | 1 TAG byte (78h) + 6 data bytes (79h-7Eh) | same | DSO32 p.96 / DSV320X p.117 |
| FIFO depth | 9 kB | 4.5 kB | changed | DSO32 p.50 / DSV320X p.45 |
| FIFO depth in words | 1024 words (7 bytes each TAG+6DATA) | 512 words (7 bytes each TAG+6DATA) | changed | DSO32 p.50 / DSV320X p.45 |

## Notes

- **Watermark width reduction**: DSO32 uses 9-bit watermark (FIFO_CTRL1[7:0] + FIFO_CTRL2[0]), while DSV320X uses 8-bit watermark (FIFO_CTRL1[7:0] only). This reduces maximum watermark from 512 to 256 words, consistent with the 50% reduction in total FIFO depth.

- **FIFO_STATUS register relocation**: FIFO_STATUS1/STATUS2 moved from addresses 3Ah/3Bh (DSO32) to 1Bh/1Ch (DSV320X), implying significant register map reorganization. This affects driver compatibility.

- **DIFF_FIFO counter width**: Reduced from 10 bits (DIFF_FIFO[9:0]) to 9 bits (DIFF_FIFO[8:0] split across STATUS1/STATUS2). DSV320X FIFO_STATUS2 no longer carries DIFF_FIFO[9:8]; only DIFF_FIFO_8 is in bit 0.

- **COUNTER_BDR_REG1 restructure**: Lost `dataready_pulsed` and `RST_COUNTER_BDR` control bits; gained `XL_HG_BATCH_EN` and expanded `TRIG_COUNTER_BDR` from 1-bit to 2-bit with new options for EIS gyro and high-g accelerometer. Counter threshold reduced from 11 bits to 10 bits.

- **Batch data rate reencoding**: Both gyroscope and accelerometer BDR tables are entirely different between parts. DSO32 mixes odd rates (12.5, 6.5, 1.6 Hz) while DSV320X uses cleaner binary multiples (1.875, 3.84, 7.68 kHz). Temperature BDR also reencoded.

- **New FIFO_MODE (010)**: DSV320X adds `continuousWTM-to-full` mode (010), replacing DSO32's reserved state. Allows continuous buffering at watermark size until trigger deasserts, then switch to full FIFO.

- **EIS and high-g support**: G_EIS_FIFO_EN bit in FIFO_CTRL4[3] enables enhanced EIS gyroscope batching; XL_HG_BATCH_EN in COUNTER_BDR_REG1[4] enables high-g accelerometer batching, with dedicated TRIG_COUNTER_BDR value (11) and new FIFO tags (0x1D high-g accel, 0x18 peak, 0x1E EIS gyro).

- **SFLP and MLC tags**: DSV320X adds 6 new algorithm-related tags (0x13, 0x16, 0x17, 0x1A-0x1C) for sensor fusion (SFLP) and machine learning core (MLC) outputs, enabling advanced post-processing results to be logged in FIFO.

- **Temperature BDR reduction**: DSV320X ODR_T_BATCH rates are lower: maximum 60 Hz vs. DSO32's 52 Hz; minimum 1.875 Hz vs. 1.6 Hz.

- **FIFO word format unchanged**: Both parts maintain 7-byte words (1 TAG + 6 DATA), accessible via same register sequence (78h-7Eh).

## Could not cite

- FIFO_CTRL4 bit 3 (G_EIS_FIFO_EN) default value in DSV320X register tables not explicitly listed in extracted table rows; inferred as 0 from description.
- COUNTER_BDR_REG1 reserved bits marked as "(1)" in both parts but rationale/retention requirement not documented in extracted sections.

## Coverage

Compared all primary FIFO control and status registers (FIFO_CTRL1-4, COUNTER_BDR_REG1-2, FIFO_STATUS1-2, FIFO_DATA_OUT_TAG) and FIFO data output registers (79h-7Eh). Extracted complete FIFO tag tables, batch data rate encodings, FIFO mode encodings, and depth specifications. Identified watermark width reduction, register address reassignment, new EIS/high-g/SFLP/MLC features, and temperature rate reencoding. Did not find documentation of FIFO timestamp format changes between parts in extracted sections (both reference 25 μs LSB per TIMESTAMP0-3 descriptions); timestamp decimation field unchanged.
