# Register delta — Self-test and calibration — LSM6DSO32 vs LSM6DSV320X

## Delta table

| name | DSO32 | DSV320X | status | page cite |
|------|-------|---------|--------|-----------|
| **Self-test enable (low-g accel/gyro)** |
| CTRL5_C (14h) ST1_XL, ST0_XL bits | Register 14h, bits [1:0]; pos/neg encoding in Table 56 | Moved to CTRL10 (19h) | changed | DSO32 p.71 / DSV320X p.76 |
| CTRL10 (19h) ST_XL_[1:0], ST_G_[1:0] bits | N/A | Register 19h, bits [3:0]; pos/neg/reserved encoding in Table 75 | new | DSO32 p.— / DSV320X p.76 |
| **Self-test modes (low-g accelerometer)** |
| ST1_XL ST0_XL encoding | 00=disabled, 01=positive, 10=negative, 11=not allowed (Table 56) | 00=normal, 01=positive, 10=negative, 11=reserved (Table 75) | changed | DSO32 p.71 / DSV320X p.76 |
| **Self-test modes (gyroscope)** |
| ST1_G ST0_G encoding | 00=disabled, 01=positive, 10=not allowed, 11=negative (Table 55) | 00=normal, 01=positive, 10=negative, 11=reserved (Table 75) | changed | DSO32 p.71 / DSV320X p.76 |
| **Self-test output limits (accel)** |
| Accelerometer self-test output change | 50–1700 mg (full-scale independent) (Table 3) | Low-g: 50–1700 mg; High-g: 150–2500 mg (full-scale independent) (Table 3) | changed | DSO32 p.25 / DSV320X p.12 |
| **Self-test output limits (gyro)** |
| Gyroscope self-test output change | FS ±250 dps: 20–80 dps; FS ±2000 dps: 150–700 dps (Table 3) | FS ±250 dps: 20–80 dps; FS ±4000 dps: 20–80 dps (Table 3) | changed | DSO32 p.25 / DSV320X p.12 |
| **High-g accelerometer self-test** |
| CTRL2_XL_HG (4Dh) XL_HG_ST[1:0] bits | N/A | Register 4Dh, bits [1:0]; 00=normal, 01=positive, 10=negative, 11=reserved (Table 146) | new | DSO32 p.— / DSV320X p.93 |
| **OIS self-test (low-g accel/gyro)** |
| UI_INT_OIS (6Fh) ST_XL_OIS_[1:0], ST_G_OIS_[1:0] bits | N/A | Register 6Fh (UI interface); 00=normal, 01=positive, 10=negative (ST_XL), varies for ST_G (Table 215) | new | DSO32 p.— / DSV320X p.139 |
| IF2_INT_OIS (6Fh) ST_XL_OIS_[1:0], ST_G_OIS_[1:0] bits | N/A | Register 6Fh (IF2 interface); 00=normal, 01=positive, 10=normal, 11=negative for ST_G; 00=normal, 01=positive, 10=negative, 11=reserved for ST_XL (Table 267) | new | DSO32 p.— / DSV320X p.145 |
| ST_OIS_CLAMPDIS bit | N/A | Register 6Fh bit [5]; 0=outputs 8000h during self-test, 1=self-test outputs (Table 267) | new | DSO32 p.— / DSV320X p.145 |
| **User offset (low-g accelerometer)** |
| X_OFS_USR (73h) address and weight | 73h, weight: 2^-10 g/LSB (USR_OFF_W=0) or 2^-6 g/LSB (USR_OFF_W=1); register in CTRL6_C (15h) | 73h, weight: 2^-10 g/LSB (USR_OFF_W=0) or 2^-6 g/LSB (USR_OFF_W=1); register in CTRL9 (18h) | same | DSO32 p.93 / DSV320X p.115 |
| Y_OFS_USR (74h) address and weight | 74h, weight: 2^-10 or 2^-6 g/LSB (CTRL6_C USR_OFF_W) | 74h, weight: 2^-10 or 2^-6 g/LSB (CTRL9 USR_OFF_W) | same | DSO32 p.94 / DSV320X p.115 |
| Z_OFS_USR (75h) address and weight | 75h, weight: 2^-10 or 2^-6 g/LSB (CTRL6_C USR_OFF_W) | 75h, weight: 2^-10 or 2^-6 g/LSB (CTRL9 USR_OFF_W) | same | DSO32 p.94 / DSV320X p.115 |
| **User offset (high-g accelerometer)** |
| XL_HG_X_OFS_USR (6Ch) | N/A | 6Ch, weight: 0.25 g/LSb (FS ≤±256 g) or 0.33 g/LSb (FS ±320 g); range [-127, 127] | new | DSO32 p.— / DSV320X p.110 |
| XL_HG_Y_OFS_USR (6Dh) | N/A | 6Dh, weight: 0.25 g/LSb (FS ≤±256 g) or 0.33 g/LSb (FS ±320 g); range [-127, 127] | new | DSO32 p.— / DSV320X p.110 |
| XL_HG_Z_OFS_USR (6Eh) | N/A | 6Eh, weight: 0.25 g/LSb (FS ≤±256 g) or 0.33 g/LSb (FS ±320 g); range [-127, 127] | new | DSO32 p.— / DSV320X p.110 |
| **User offset control** |
| USR_OFF_W bit location | CTRL6_C (15h) | CTRL9 (18h) | changed | DSO32 p.72 / DSV320X p.74 |
| USR_OFF_ON_OUT bit location | CTRL7_G (16h) | CTRL9 (18h) | changed | DSO32 p.73 / DSV320X p.74 |
| HG_USR_OFF_ON_OUT bit | N/A | CTRL1_XL_HG (4Eh), enables high-g user offset on output registers | new | DSO32 p.— / DSV320X p.93 |
| **Gyro offset registers** |
| Gyroscope user offset registers | Not found | Not found | removed | DSO32 p.— / DSV320X p.— |
| **Temperature sensor** |
| OUT_TEMP_L (20h), OUT_TEMP_H (21h) address | 20h, 21h, 16-bit two's complement | 20h, 21h, 16-bit two's complement | same | DSO32 p.79 / DSV320X p.80 |
| Temperature sensor sensitivity (TSen) | 256 LSB/°C | 256 LSB/°C | same | DSO32 p.29 / DSV320X p.16 |
| Temperature sensor offset (Toff) | -15 to +15 °C | -15 to +15 °C | same | DSO32 p.29 / DSV320X p.16 |
| Temperature sensor refresh rate (TODR) | 52 Hz | 60 Hz | changed | DSO32 p.29 / DSV320X p.16 |
| Temperature output at 25°C | 0 LSB (typ.) | 0 LSB (typ.) | same | DSO32 p.29 / DSV320X p.16 |

## Notes

- **Self-test register move (DSO32→DSV320X)**: Self-test enable bits for low-g accel/gyro moved from CTRL5_C (14h) in DSO32 to CTRL10 (19h) in DSV320X. Encoding also changed: DSO32 used "disabled" as 00 state, DSV320X uses "normal mode" as 00. DSO32 has "not allowed" states; DSV320X uses "reserved" terminology.
- **High-g accelerometer self-test**: DSV320X adds dedicated self-test control for the high-g accelerometer via CTRL2_XL_HG (4Dh), which has no equivalent in DSO32.
- **OIS chain self-test**: DSV320X adds separate OIS (optical image stabilization / output from the OIS path) self-test controls via UI_INT_OIS (6Fh) and IF2_INT_OIS (6Fh) with ST_OIS_CLAMPDIS control for output clamping during self-test.
- **Self-test output limits**: DSO32 specifies single accel limit (50–1700 mg); DSV320X distinguishes low-g (50–1700 mg) and high-g (150–2500 mg). Gyro limits differ: DSO32 at FS ±2000 dps gives 150–700 dps; DSV320X at FS ±4000 dps gives 20–80 dps (narrower range, higher FS).
- **User offset register move**: Low-g accel offset weight register moved from CTRL6_C (15h) to CTRL9 (18h). USR_OFF_ON_OUT bit moved from CTRL7_G (16h) to CTRL9 (18h).
- **High-g accel offset**: DSV320X adds dedicated high-g offset registers (6Ch, 6Dh, 6Eh) with full-scale–dependent weights (0.25 or 0.33 g/LSb). DSO32 has no high-g accelerometer.
- **No gyro offset**: Neither part provides user-programmable gyroscope offset registers.
- **Temperature sensor**: Address, sensitivity, and offset characteristics identical. TODR (refresh rate) increased from 52 Hz (DSO32) to 60 Hz (DSV320X).
- **Factory calibration note**: Both datasheets state that registers loaded at boot contain factory calibration values and are automatically restored on power-up; user should not change them.

## Coverage

Compared register maps, control registers (CTRL5_C, CTRL6_C, CTRL7_G, CTRL9, CTRL10, CTRL2_XL_HG, UI_INT_OIS, IF2_INT_OIS), user offset registers (X/Y/Z_OFS_USR, XL_HG_X/Y/Z_OFS_USR), temperature output (OUT_TEMP_L/H), and associated specification tables (self-test modes, self-test output change limits, sensor characteristics). No gyroscope user offset or bias trim registers found in either part.
