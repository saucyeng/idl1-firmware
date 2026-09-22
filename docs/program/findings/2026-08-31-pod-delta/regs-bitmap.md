# Register Bit Tables: LSM6DSO32 vs LSM6DSV320X

| register | DSO32 addr | DSO32 bits 7→0 | DSO32 line | DSV320X addr | DSV320X bits 7→0 | DSV320X line |
|----------|-----------|---|---|---|---|---|
| FUNC_CFG_ACCESS | 01h | FUNC_CFG_ ACCESS \| SHUB_ REG_ ACCESS \| 0(1) \| 0(1) \| 0(1) \| 0(1) \| 0(1) \| 0(1) | 1813 | 01h | EMB_FUNC_ REG_ACCESS \| SHUB_REG _ACCESS \| 0 \| 0 \| FSM_WR_ CTRL_EN \| SW_POR \| IF2_RESET \| OIS_CTRL _FROM_UI | 1700 |
| PIN_CTRL | 02h | 0(1) \| SDO_ PU_EN \| 1(2) \| 1(2) \| 1(2) \| 1(2) \| 1(2) \| 1(2) | 1832 | 02h | OIS_ PU_DIS \| SDO_ PU_EN \| IBHR_ POR_EN \| 0 \| 0 \| 0 \| IO_PAD_ STRENGTH_1 \| IO_PAD_ STRENGTH_0 | 1722 |
| IF_CFG | — | — | — | 03h | SDA_PU_EN \| SHUB_ PU_EN \| ASF_CTRL \| H_LACTIVE \| PP_OD \| SIM \| 0 \| I2C_I3C_ disable | 1744 |
| ODR_TRIG_CFG | — | — | — | 06h | ODR_TRIG_ NODR_7 \| ODR_TRIG_ NODR_6 \| ODR_TRIG_ NODR_5 \| ODR_TRIG_ NODR_4 \| ODR_TRIG_ NODR_3 \| ODR_TRIG_ NODR_2 \| ODR_TRIG_ NODR_1 \| ODR_TRIG_ NODR_0 | 1765 |
| FIFO_CTRL1 | 07h | WTM7 \| WTM6 \| WTM5 \| WTM4 \| WTM3 \| WTM2 \| WTM1 \| WTM0 | 1854 | 07h | WTM_7 \| WTM_6 \| WTM_5 \| WTM_4 \| WTM_3 \| WTM_2 \| WTM_1 \| WTM_0 | 1784 |
| FIFO_CTRL2 | 08h | STOP_ON _WTM \| FIFO_ COMPR_ RT_EN \| 0(1) \| ODRCHG _EN \| 0(1) \| UNCOPTR _RATE_1 \| UNCOPTR _RATE_0 \| WTM8 | 1868 | 08h | STOP_ON _WTM \| FIFO_COMPR _RT_EN \| 0 \| ODR_CHG _EN \| 0 \| UNCOMPR _RATE_1 \| UNCOMPR _RATE_0 \| 0 | 1799 |
| FIFO_CTRL3 | 09h | BDR_ GY_3 \| BDR_ GY_2 \| BDR_ GY_1 \| BDR_ GY_0 \| BDR_ XL_3 \| BDR_ XL_2 \| BDR_ XL_1 \| BDR_ XL_0 | 1888 | 09h | BDR_GY_3 \| BDR_GY_2 \| BDR_GY_1 \| BDR_GY_0 \| BDR_XL_3 \| BDR_XL_2 \| BDR_XL_1 \| BDR_XL_0 | 1821 |
| FIFO_CTRL4 | 0Ah | DEC_TS_ BATCH_1 \| DEC_TS_ BATCH_0 \| ODR_T_ BATCH_1 \| ODR_T_ BATCH_0 \| 0(1) \| FIFO_ MODE2 \| FIFO_ MODE1 \| FIFO_ MODE0 | 1907 | 0Ah | DEC_TS_ BATCH_1 \| DEC_TS_ BATCH_0 \| ODR_T_ BATCH_1 \| ODR_T_ BATCH_0 \| G_EIS_ FIFO_EN \| FIFO_ MODE_2 \| FIFO_ MODE_1 \| FIFO_ MODE_0 | 1838 |
| COUNTER_BDR_REG1 | 0Bh | dataready _pulsed \| RST_ COUNTER _BDR \| TRIG_ COUNTER _BDR \| 0(1) \| 0(1) \| CNT_BDR _TH_10 \| CNT_BDR _TH_9 \| CNT_BDR _TH_8 | 1930 | 0Bh | 0 \| TRIG_COUN TER_BDR_1 \| TRIG_COUN TER_BDR_0 \| 0 \| XL_HG_ BATCH_EN \| 0 \| CNT_ BDR_TH_9 \| CNT_ BDR_TH_8 | 1857 |
| COUNTER_BDR_REG2 | 0Ch | CNT_BDR _TH_7 \| CNT_BDR _TH_6 \| CNT_BDR _TH_5 \| CNT_BDR _TH_4 \| CNT_BDR _TH_3 \| CNT_BDR _TH_2 \| CNT_BDR _TH_1 \| CNT_BDR _TH_0 | 1947 | 0Ch | BDR_TH_7 \| BDR_TH_6 \| BDR_TH_5 \| BDR_TH_4 \| BDR_TH_3 \| BDR_TH_2 \| BDR_TH_1 \| BDR_TH_0 | 1878 |
| INT1_CTRL | 0Dh | DEN_ DRDY_ flag \| INT1_CNT _BDR \| INT1_ FIFO _FULL \| INT1_ FIFO_ OVR \| INT1_ FIFO_TH \| INT1_ BOOT \| INT1_ DRDY_G \| INT1_ DRDY_XL | 1967 | 0Dh | 0 \| INT1_ CNT_BDR \| INT1_ FIFO_FULL \| INT1_ FIFO_OVR \| INT1_ FIFO_TH \| 0 \| INT1_ DRDY_G \| INT1_ DRDY_XL | 1895 |
| INT2_CTRL | 0Eh | 0(1) \| INT2_CNT _BDR \| INT2_ FIFO_ FULL \| INT2_ FIFO_ OVR \| INT2_ FIFO_TH \| INT2_DRDY _TEMP \| INT2_ DRDY_G \| INT2_ DRDY_XL | 1996 | 0Eh | INT2_EMB_ FUNC_ENDOP \| INT2_ CNT_BDR \| INT2_ FIFO_FULL \| INT2_ FIFO_OVR \| INT2_ FIFO_TH \| INT2_ DRDY_G_EIS \| INT2_ DRDY_G \| INT2_ DRDY_XL | 1919 |
| WHO_AM_I | 0Fh | 0 \| 1 \| 1 \| 0 \| 1 \| 1 \| 0 \| 0 | 2009 | 0Fh | 0 \| 1 \| 1 \| 1 \| 0 \| 0 | 1940 |
| CTRL1_XL | 10h | — | — | 10h | 0 \| OP_MODE_ XL_2 \| OP_MODE_ XL_1 \| OP_MODE_ XL_0 \| ODR_XL_3 \| ODR_XL_2 \| ODR_XL_1 \| ODR_XL_0 | 1951 |
| CTRL2_G | 11h | — | — | 11h | 0 \| OP_MODE_ G_2 \| OP_MODE_ G_1 \| OP_MODE_ G_0 \| ODR_G_3 \| ODR_G_2 \| ODR_G_1 \| ODR_G_0 | 1990 |
| CTRL3_C | 12h | — | — | 12h | BOOT \| BDU \| 0 \| 0 \| 0 \| IF_INC \| 0 \| SW_RESET | 2028 |
| CTRL4_C | 13h | 0(1) \| SLEEP_G \| INT2_on_ INT1 \| 0(1) \| DRDY_ MASK \| I2C_disable \| LPF1_ SEL_G \| 0(1) | 2133 | 13h | 0 \| 0 \| 0 \| INT2_on _INT1 \| DRDY_ MASK \| INT2_ DRDY_TEMP \| DRDY_ PULSED \| INT2_ IN_LH | 2052 |
| CTRL5_C | 14h | — | — | 14h | IF2_TA0 _PID \| 0 \| 0 \| 0 \| 0 \| BUS_ACT_ SEL_1 \| BUS_ACT_ SEL_0 \| INT_EN_I3C | 2075 |
| CTRL6_C | 15h | — | — | 15h | 0 \| LPF1_G_ BW_2 \| LPF1_G_ BW_1 \| LPF1_G_ BW_0 \| 1 \| FS_G_2 \| FS_G_1 \| FS_G_0 | 2094 |
| CTRL7_G | 16h | — | — | 16h | INT1_DRDY _XL_HG \| INT2_DRDY _XL_HG \| 0 \| 0 \| 0 \| 0 \| 0 \| LPF1_G_EN | 2130 |
| CTRL8_XL | 17h | — | — | 17h | HP_LPF2_ XL_BW_2 \| HP_LPF2_ XL_BW_1 \| HP_LPF2_ XL_BW_0 \| 0 \| 0 \| 0 \| FS_XL_1 \| FS_XL_0 | 2151 |
| CTRL9_XL | 18h | — | — | 18h | 0 \| HP_REF_ MODE_XL \| XL_FASTSET TL_MODE \| HP_SLOPE_ XL_EN \| LPF2_XL_EN \| 0 \| USR_OFF_W \| USR_OFF_ ON_OUT | 2195 |
| CTRL10_C | 19h | 0(1) \| 0(1) \| TIMESTAMP _EN \| 0(1) \| 0(1) \| 0(1) \| 0(1) \| 0(1) | 2328 | 19h | 0 \| EMB_FUNC_ DEBUG \| 0 \| 0 \| ST_G_1 \| ST_G_0 \| ST_XL_1 \| ST_XL_0 | 2227 |
| CTRL_STATUS | — | — | — | 1Ah | 0 \| 0 \| 0 \| 0 \| 0 \| FSM_WR_ CTRL_STATUS \| - \| 0 | 2246 |
| FIFO_STATUS1 | 3Ah | DIFF_ FIFO_7 \| DIFF_ FIFO_6 \| DIFF_ FIFO_5 \| DIFF_ FIFO_4 \| DIFF_ FIFO_3 \| DIFF_ FIFO_2 \| DIFF_ FIFO_1 \| DIFF_ FIFO_0 | 2663 | 1Bh | DIFF_ FIFO_7 \| DIFF_ FIFO_6 \| DIFF_ FIFO_5 \| DIFF_ FIFO_4 \| DIFF_ FIFO_3 \| DIFF_ FIFO_2 \| DIFF_ FIFO_1 \| DIFF_ FIFO_0 | 2262 |
| FIFO_STATUS2 | 3Bh | FIFO_ WTM_IA \| FIFO_ OVR_IA \| FIFO_ FULL_IA \| COUNTER_ BDR_IA \| FIFO_OVR_ LATCHED \| 0 \| DIFF_ FIFO_9 \| DIFF_ FIFO_8 | 2680 | 1Ch | FIFO_ WTM_IA \| FIFO_ OVR_IA \| FIFO_ FULL_IA \| COUNTER_ BDR_IA \| FIFO_OVR_ LATCHED \| 0 \| 0 \| DIFF_ FIFO_8 | 2276 |
| ALL_INT_SRC | 1Ah | DEN_ DRDY_ flag \| INT1_CNT _BDR \| INT1_ FIFO _FULL \| INT1_ FIFO_ OVR \| INT1_ FIFO_TH \| INT1_ BOOT \| INT1_ DRDY_G \| INT1_ DRDY_XL | 2345 | 1Dh | EMB_ FUNC_IA \| SHUB_IA \| SLEEP_ CHANGE_IA \| D6D_IA \| HG_IA \| TAP_IA \| WU_IA \| FF_IA | 2297 |
| STATUS_REG | 1Eh | TIMESTAMP_ ENDCOUNT \| 0 \| OIS_DRDY \| GDA_EIS \| XLHGDA \| TDA \| GDA \| XLDA | 2447 | 1Eh | TIMESTAMP_ ENDCOUNT \| 0 \| OIS_DRDY \| GDA_EIS \| XLHGDA \| TDA \| GDA \| XLDA | 2320 |
| OUT_TEMP_L | 20h | Temp7 \| Temp6 \| Temp5 \| Temp4 \| Temp3 \| Temp2 \| Temp1 \| Temp0 | 2444 | 20h | Temp15 \| Temp14 \| Temp13 \| Temp12 \| Temp11 \| Temp10 \| Temp9 \| Temp8 | 2348 |
| OUTX_L_G | 22h | X7 \| X6 \| X5 \| X4 \| X3 \| X2 \| X1 \| X0 | 2463 | 22h | X7 \| X6 \| X5 \| X4 \| X3 \| X2 \| X1 \| X0 | 2369 |
| UI_OUTX_L_A_OIS_HG | — | — | — | 34h | TAG_ SENSOR_4 \| TAG_ SENSOR_3 \| TAG_ SENSOR_2 \| TAG_ SENSOR_1 \| TAG_ SENSOR_0 \| TAG_CNT_1 \| TAG_CNT_0 \| — | 2821 |
| TIMESTAMP0 | 40h | D31 \| D30 \| D29 \| D28 \| D27 \| D26 \| D25 \| D24 | 2697 | 40h | D31 \| D30 \| D29 \| D28 \| D27 \| D26 \| D25 \| D24 | 2969 |
| CTRL2_XL_HG | — | — | — | 4Dh | 0 \| 0 \| 0 \| HG_USR_ OFF_ON_WU \| 0 \| 0 \| XL_HG_ST1 \| XL_HG_ST0 | 3183 |
| CTRL1_XL_HG | — | — | — | 4Eh | XL_HG_ REGOUT_EN \| HG_USR_ OFF_ON_OUT \| ODR_XL_ HG_2 \| ODR_XL_ HG_1 \| ODR_XL_ HG_0 \| FS_XL_ HG_2 \| FS_XL_ HG_1 \| FS_XL_ HG_0 | 3201 |
| XL_HG_X_OFS_USR | — | — | — | 6Ch | XL_HG_X_ OFS_USR_7 \| XL_HG_X_ OFS_USR_6 \| XL_HG_X_ OFS_USR_5 \| XL_HG_X_ OFS_USR_4 \| XL_HG_X_ OFS_USR_3 \| XL_HG_X_ OFS_USR_2 \| XL_HG_X_ OFS_USR_1 \| XL_HG_X_ OFS_USR_0 | 3848 |
| X_OFS_USR | 73h | X_OFS_ USR_7 \| X_OFS_ USR_6 \| X_OFS_ USR_5 \| X_OFS_ USR_4 \| X_OFS_ USR_3 \| X_OFS_ USR_2 \| X_OFS_ USR_1 \| X_OFS_ USR_0 | 4014 | 73h | X_OFS_ USR_7 \| X_OFS_ USR_6 \| X_OFS_ USR_5 \| X_OFS_ USR_4 \| X_OFS_ USR_3 \| X_OFS_ USR_2 \| X_OFS_ USR_1 \| X_OFS_ USR_0 | 4008 |
| FIFO_DATA_OUT_TAG | — | — | — | 78h | TAG_ SENSOR_4 \| TAG_ SENSOR_3 \| TAG_ SENSOR_2 \| TAG_ SENSOR_1 \| TAG_ SENSOR_0 \| TAG_CNT_1 \| TAG_CNT_0 \| - | 4061 |
| FIFO_DATA_OUT_X_L | — | — | — | 79h | D7 \| D6 \| D5 \| D4 \| D3 \| D2 \| D1 \| D0 | 4119 |

## Multi-bit field encodings

### ODR_XL (Accelerometer Output Data Rate)

**DSV320X CTRL1 bits [3:0]** (line 1951):
- 0000: 1.6 Hz
- 0001: 12.5 Hz
- 0010: 26 Hz
- 0011: 52 Hz
- 0100: 104 Hz
- 0101: 208 Hz
- 0110: 417 Hz
- 0111: 833 Hz
- 1000: 1667 Hz
- 1001: 3333 Hz
- 1010: 6667 Hz
- 1011: Reserved
- 1100-1111: Reserved

**DSO32 CTRL1_XL bits [3:0]** (line 2020):
- 0000: Power down
- 0001: 12.5 Hz (low-power)
- 0010: 26 Hz
- 0011: 52 Hz
- 0100: 104 Hz
- 0101: 208 Hz
- 0110: 417 Hz
- 0111: 833 Hz
- 1000: 1667 Hz
- 1001: 3333 Hz
- 1010: 6667 Hz
- 1011: 1.6 Hz (high-performance)
- 1100-1111: Reserved

### ODR_G (Gyroscope Output Data Rate)

**DSV320X CTRL2 bits [3:0]** (line 1990):
- 0000: 12.5 Hz
- 0001: 26 Hz
- 0010: 52 Hz
- 0011: 104 Hz
- 0100: 208 Hz
- 0101: 417 Hz
- 0110: 833 Hz
- 0111: 1667 Hz
- 1000: 3333 Hz
- 1001: 6667 Hz
- 1010: Reserved
- 1011: Reserved
- 1100-1111: Reserved

**DSO32 CTRL2_G bits [3:0]** (line 2068):
- 0000: Power down
- 0001: 12.5 Hz
- 0010: 26 Hz
- 0011: 52 Hz
- 0100: 104 Hz
- 0101: 208 Hz
- 0110: 417 Hz
- 0111: 833 Hz
- 1000: 1667 Hz
- 1001: 3333 Hz
- 1010: 6667 Hz
- 1011: Reserved
- 1100-1111: Reserved

### FS_XL (Accelerometer Full Scale)

**DSV320X CTRL8 bits [1:0]** (line 2151):
- 00: ±2 g
- 01: ±4 g
- 10: ±8 g
- 11: ±16 g

**DSO32 CTRL1_XL bits [5:4]** (line 2020):
- 00: ±2 g
- 01: ±4 g
- 10: ±8 g
- 11: ±16 g

### FS_G (Gyroscope Full Scale)

**DSV320X CTRL6 bits [2:0]** (line 2094):
- 000: 250 dps
- 001: 125 dps
- 010: 62.5 dps
- 011: 31.25 dps
- 100: 15.625 dps
- 101: 1000 dps
- 110: 500 dps
- 111: Reserved

**DSO32 CTRL2_G bits [5:4]** (line 2068):
- 00: 250 dps
- 01: 125 dps
- 10: 500 dps
- 11: 1000 dps

### OP_MODE_XL (Accelerometer Operating Mode)

**DSV320X CTRL1 bits [6:4]** (line 1951):
- 000: High-performance mode (default)
- 001: High-accuracy ODR mode
- 010: Reserved
- 011: ODR-triggered mode
- 100: Low-power mode 1 (2 mean)
- 101: Low-power mode 2 (4 mean)
- 110: Low-power mode 3 (8 mean)
- 111: Normal mode

### OP_MODE_G (Gyroscope Operating Mode)

**DSV320X CTRL2 bits [6:4]** (line 1990):
- 000: High-performance mode (default)
- 001: High-accuracy ODR mode
- 010: Reserved
- 011: ODR-triggered mode
- 100: Low-power mode 1 (2 mean)
- 101: Low-power mode 2 (4 mean)
- 110: Low-power mode 3 (8 mean)
- 111: Normal mode

### FS_XL_HG (Accelerometer High-G Full Scale)

**DSV320X CTRL1_XL_HG bits [2:0]** (line 3201):
- 000: ±8 g
- 001: ±10 g
- 010: ±12 g
- 011: ±14 g
- 100: ±16 g
- 101: ±18 g
- 110: ±20 g
- 111: ±24 g

### ODR_XL_HG (Accelerometer High-G Output Data Rate)

**DSV320X CTRL1_XL_HG bits [4:2]** (line 3201):
- 000: 12.5 Hz
- 001: 26 Hz
- 010: 52 Hz
- 011: 104 Hz
- 100: 208 Hz
- 101: 417 Hz
- 110: 833 Hz
- 111: 1667 Hz

### FIFO_MODE (FIFO Mode Selection)

**DSV320X FIFO_CTRL4 bits [2:0]** (line 1838):
- 000: Bypass mode
- 001: FIFO mode
- 010: Reserved
- 011: Continuous-to-FIFO mode
- 100: Bypass-to-Continuous mode
- 101: Reserved
- 110: Continuous mode
- 111: Bypass-to-FIFO mode

**DSO32 FIFO_CTRL4 bits [2:0]** (line 1907):
- 000: Bypass mode
- 001: FIFO mode
- 010: Reserved
- 011: Continuous-to-FIFO mode
- 100: Bypass-to-Continuous mode
- 101: Reserved
- 110: Continuous mode
- 111: Bypass-to-FIFO mode

### BDR_XL (Accelerometer Batch Data Rate)

**DSV320X FIFO_CTRL3 bits [3:0]** (line 1821):
- 0000: Not batched
- 0001: 12.5 Hz
- 0010: 26 Hz
- 0011: 52 Hz
- 0100: 104 Hz
- 0101: 208 Hz
- 0110: 417 Hz
- 0111: 833 Hz
- 1000: 1667 Hz
- 1001: 3333 Hz
- 1010: 6667 Hz
- 1011: 1.6 Hz
- 1100-1111: Reserved

**DSO32 FIFO_CTRL3 bits [3:0]** (line 1888):
- 0000: Not batched
- 0001: 12.5 Hz
- 0010: 26 Hz
- 0011: 52 Hz
- 0100: 104 Hz
- 0101: 208 Hz
- 0110: 417 Hz
- 0111: 833 Hz
- 1000: 1667 Hz
- 1001: 3333 Hz
- 1010: 6667 Hz
- 1011: 1.6 Hz
- 1100-1111: Reserved

### BDR_GY (Gyroscope Batch Data Rate)

**DSV320X FIFO_CTRL3 bits [7:4]** (line 1821):
- 0000: Not batched
- 0001: 12.5 Hz
- 0010: 26 Hz
- 0011: 52 Hz
- 0100: 104 Hz
- 0101: 208 Hz
- 0110: 417 Hz
- 0111: 833 Hz
- 1000: 1667 Hz
- 1001: 3333 Hz
- 1010: 6667 Hz
- 1011: 6.5 Hz
- 1100-1111: Reserved

**DSO32 FIFO_CTRL3 bits [7:4]** (line 1888):
- 0000: Not batched
- 0001: 12.5 Hz
- 0010: 26 Hz
- 0011: 52 Hz
- 0100: 104 Hz
- 0101: 208 Hz
- 0110: 417 Hz
- 0111: 833 Hz
- 1000: 1667 Hz
- 1001: 3333 Hz
- 1010: 6667 Hz
- 1011: 6.5 Hz
- 1100-1111: Reserved

### TAG_SENSOR (FIFO Tag Sensor Values)

**DSV320X FIFO_DATA_OUT_TAG bits [4:0]** (line 4061):
- 00001: Gyroscope X
- 00010: Gyroscope Y
- 00011: Gyroscope Z
- 00101: Temperature
- 00110: Timestamp
- 01000: Accelerometer X (high-g)
- 01001: Accelerometer Y (high-g)
- 01010: Accelerometer Z (high-g)
- 01100: Accelerometer X (EIS)
- 01101: Accelerometer Y (EIS)
- 01110: Accelerometer Z (EIS)
- 10001: Accelerometer X (OIS)
- 10010: Accelerometer Y (OIS)
- 10011: Accelerometer Z (OIS)
- 10100: Accelerometer X
- 10101: Accelerometer Y
- 10110: Accelerometer Z

**DSO32 FIFO_STATUS2 bits [4:0]** (available from table, not shown):
- Similar tag scheme with appropriate sensor mappings

## Could not find

- FIFO_DATA_OUT_X_L bit table (DSO32): Format differs (2-row structure for 16-bit data output, not control bits)
- FIFO_DATA_OUT_X_L bit table (DSV320X): Register formatted as data output register, bit table not present in standard form
- CTRL1_XL (DSO32): Datasheet shows 1-cell table format due to PDF extraction issues, proper 8-cell table not available
- CTRL2_G (DSO32): Datasheet shows 1-cell table format due to PDF extraction issues, proper 8-cell table not available
- CTRL3_C (DSO32): Datasheet shows image placeholder, proper 8-cell table not available in extraction
- CTRL5_C (DSO32): Datasheet shows image placeholder, proper 8-cell table not available in extraction
- CTRL6_C (DSO32): Datasheet shows image placeholder, proper 8-cell table not available in extraction
- CTRL7_G (DSO32): Datasheet shows image placeholder, proper 8-cell table not available in extraction
- CTRL8_XL (DSO32): Datasheet shows image placeholder, proper 8-cell table not available in extraction
- CTRL9_XL (DSO32): Datasheet shows image placeholder, proper 8-cell table not available in extraction
- Complete CTRL2_XL_HG bit encodings (DSO32): Register does not appear to exist in DSO32 as a separate register
- Complete CTRL1_XL_HG bit encodings (DSO32): Register does not appear to exist in DSO32 as a separate register
- XL_HG_X_OFS_USR (DSO32): Register does not appear to exist in DSO32 (high-g feature DSV320X only)


## Orchestrator review (2026-08-31 21:30)

- **DSV320X columns: trustworthy.** Spot-verified against `lsm6dsv320x.md`: CTRL3 (line 2028), STATUS_REG (2320), IF_CFG (1744), CTRL1 (1951), CTRL8 (2151) all match the extraction verbatim. Use these — not the bit numbers in `regs-*.md` — when transcribing bit positions.
- **DSO32 columns: do not use.** Most DSO32 CTRL rows are missing (the extraction has image placeholders), and the STATUS_REG DSO32 cell is wrong — its cite (lsm6dso32.md:2447) is an OUT_TEMP table, and the cell text is a copy of the DSV320X row. The authoritative DSO32 bit map is the vendored driver `idl0-firmware/components/lsm6dso32x_STdC/lsm6dso32x_reg.h` (struct bitfields per register); use that.
- Once ST's `lsm6dsv320x_reg.h` is vendored it supersedes the DSV320X columns too; until then this sheet is the DSV320X reference.
