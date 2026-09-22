# Register delta — I2C interface specifics — LSM6DSO32 vs LSM6DSV320X

## Delta table

| name | DSO32 | DSV320X | status | page cite |
|------|-------|---------|--------|-----------|
| I2C role | Slave | Target | changed | DSO32 p.34 / DSV320X p.51 |
| Slave address base address | 110101xb | 110101xb | same | DSO32 p.34 / DSV320X p.51 |
| Address LSB control pin | SDO/SA0 | SDO/TA0 | changed | DSO32 p.34 / DSV320X p.51 |
| Address when LSB pin = 0 (GND) | 1101010 (0xD4 write, 0xD5 read) | 1101010 (0xD4 write, 0xD5 read) | same | DSO32 p.34 / DSV320X p.51 |
| Address when LSB pin = 1 (VDDIO) | 1101011 (0xD6 write, 0xD7 read) | 1101011 (0xD6 write, 0xD7 read) | same | DSO32 p.34 / DSV320X p.51 |
| CS pin for I2C selection | Tied high to VDDIO for I2C | Tied high to VDDIO for I2C | same | DSO32 p.34 / DSV320X p.51 |
| I2C speed modes supported | Standard mode, Fast mode (400 kHz) | Standard mode, Fast mode (400 kHz), Fast mode plus (1000 kHz) | changed | DSO32 p.31 / DSV320X p.35 |
| I2C maximum SCL frequency | 400 kHz | 1000 kHz (1 MHz) | changed | DSO32 p.31 / DSV320X p.35 |
| I2C timing table: f(SCL) range | 0–400 kHz | 0–400 kHz (fast mode), 0–1000 kHz (fast mode plus) | changed | DSO32 p.31 / DSV320X p.35 |
| I2C timing: t_w(SCLL) at fast mode | 4.7 µs (standard), 1.3 µs (fast) | 1.3 µs (fast mode), 0.5 µs (fast mode plus) | changed | DSO32 p.31 / DSV320X p.35 |
| I2C timing: t_w(SCLH) at fast mode | 4.0 µs (standard), 0.6 µs (fast) | 0.6 µs (fast mode), 0.26 µs (fast mode plus) | changed | DSO32 p.31 / DSV320X p.35 |
| I2C timing: t_su(SDA) setup time | 250 ns (standard), 100 ns (fast) | 100 ns (fast mode), 50 ns (fast mode plus) | changed | DSO32 p.31 / DSV320X p.35 |
| I2C timing: t_h(SDA) hold time | 0–3.45 µs (standard), 0–0.9 µs (fast) | 0–0.9 µs (fast mode), 0 µs (fast mode plus) | changed | DSO32 p.31 / DSV320X p.35 |
| Address auto-increment (IF_INC) default | 1 (enabled) | 1 (enabled) | same | DSO32 p.69 / DSV320X p.69 |
| Address auto-increment register and bit | CTRL3_C (12h), IF_INC bit | CTRL3 (12h), IF_INC bit | same | DSO32 p.69 / DSV320X p.69 |
| Multi-byte read protocol | ST, SAD+W, SUB, SR, SAD+R, DATA, NMAK, SP | ST, TAD+W, SUB, SR, TAD+R, DATA, NCAK, SP | changed | DSO32 p.34 / DSV320X p.51 |
| I3C support | MIPI I3C SM v1.0 | MIPI I3C v1.1 | changed | DSO32 p.41 / DSV320X p.52 |
| I3C enabled by default | Yes (when INT1 floating) | Yes (enabled by default) | changed | DSO32 p.43 / DSV320X p.52 |
| I3C dual-mode operation | I2C/I3C coexistence controlled by INT1 pin at power-up | I2C/I3C both active by default; antispike filter management available | changed | DSO32 p.43 / DSV320X p.52 |
| I2C_disable register and position | CTRL4_C (13h), bit I2C_disable | IF_CFG (03h), bit I2C_I3C_disable | changed | DSO32 p.70 / DSV320X p.60 |
| I2C_disable default value | 0 (I2C enabled) | 0 (I2C and I3C enabled) | same | DSO32 p.70 / DSV320X p.60 |
| I3C_disable register | CTRL5_C (14h), I3C_disable bit (for I2C/I3C coexistence control) | IF_CFG (03h), I2C_I3C_disable bit (single control for both) | changed | DSO32 p.70 / DSV320X p.60 |
| SDO/SA0 / SDO/TA0 pull-up enable register | PIN_CTRL (02h), SDO_PU_EN bit | PIN_CTRL (02h), SDO_PU_EN bit | same | DSO32 p.69 / DSV320X p.59 |
| SDA pull-up enable | Not directly configurable in main registers for I2C | IF_CFG (03h), SDA_PU_EN bit | new | — / DSV320X p.60 |
| SCL pull-up enable | Not directly configurable | Not directly configurable | same | DSO32 p.34 / DSV320X p.51 |
| SHUB_PU_EN (auxiliary I2C master pull-up) register | CTRL5_C (14h) in sensor hub registers | IF_CFG (03h) | changed | DSO32 p.70 / DSV320X p.60 |
| Internal pull-up value range | 30 kΩ to 50 kΩ (VDDIO-dependent) | 30 kΩ to 50 kΩ (VDDIO-dependent) | same | DSO32 p.34 / DSV320X p.51 |
| VDDIO range for I2C pins | 1.62 V to 3.6 V | 1.08 V to 3.6 V | changed | DSO32 p.30 / DSV320X p.30 |
| SDA/SCL external pull-up requirement | Yes, external pull-ups required (VDDIO) | Yes, external pull-ups required (VDDIO) | same | DSO32 p.34 / DSV320X p.51 |
| I2C line voltage levels | Both VDDIO and GND | Both VDDIO and GND | same | DSO32 p.34 / DSV320X p.51 |
| Antispike filter on SDA/SCL | Present; filters SDA/SCL for I2C operation | Present; managed by protocol or always-on via ASF_CTRL bit in IF_CFG | new | DSO32 p.43 / DSV320X p.52 |
| I2C timing measurement points | 0.2·VDDIO and 0.8·VDDIO | 0.3·VDDIO and 0.7·VDDIO | changed | DSO32 p.31 / DSV320X p.35 |
| Bus capacitive load | Not specified in I2C timing table | 400 pF (fast mode), 550 pF (fast mode plus) | new | — / DSV320X p.35 |

## Notes

- **I3C coexistence**: DSO32 uses INT1 pin state at power-up to select between pure I2C mode (INT1 floating) or I2C/I3C coexistence mode (INT1 not floating). DSV320X has I2C and I3C enabled by default and includes an antispike filter management mechanism via the ASF_CTRL bit in IF_CFG (03h) for improved I2C/I3C compatibility.
  
- **I2C disable semantics**: DSO32 separates I2C_disable (CTRL4_C) and I3C_disable (CTRL5_C) bits, allowing independent disabling of each interface. DSV320X combines both with a single I2C_I3C_disable bit in IF_CFG (03h), disabling both interfaces together.
  
- **Pull-up configuration**: DSO32 uses PIN_CTRL (02h) and sensor hub registers (CTRL5_C 14h) for pull-up control. DSV320X consolidates most pull-up controls in IF_CFG (03h) (SDA_PU_EN, SHUB_PU_EN) and PIN_CTRL (02h) (SDO_PU_EN, OIS_PU_DIS), providing more granular control of SDA pull-up independent of SDO.
  
- **VDDIO extended range**: DSV320X supports VDDIO down to 1.08 V compared to DSO32's 1.62 V minimum, enabling operation in lower-voltage systems. Both maintain the same 3.6 V maximum.
  
- **Fast mode plus addition**: DSV320X adds Fast mode plus (1 MHz) support with proportionally tighter I2C timing parameters (t_w(SCLL) 0.5 µs, t_w(SCLH) 0.26 µs, t_su(SDA) 50 ns) compared to fast mode. DSO32 supports only standard and fast modes.
  
- **I2C timing measurement reference**: DSO32 uses 0.2·VDDIO / 0.8·VDDIO while DSV320X uses 0.3·VDDIO / 0.7·VDDIO as thresholds for timing measurements, affecting rise/fall time characterization.
  
- **Multi-byte I2C read protocol terminology**: DSO32 uses "slave address" (SAD), "slave acknowledge" (SAK), "no master acknowledge" (NMAK). DSV320X uses "target address" (TAD), "target acknowledge" (TAK), "no controller acknowledge" (NCAK). Protocol is functionally identical.
  
- **Register address changes**: CTRL4_C (13h) in DSO32 becomes part of a consolidated IF_CFG (03h) in DSV320X. The I2C_disable function moves from bit 1 of CTRL4_C to bit 0 of IF_CFG but retains equivalent control logic.
  
- **I2C role terminology update**: Both are bus targets/slaves functionally, but DSV320X formally uses "I2C target" terminology (I2C v2.1 standard terminology) whereas DSO32 uses "I2C slave" (older terminology).
  
- **No timing wait requirement documented**: Neither datasheet specifies a minimum wait time between consecutive I2C writes or between reading multi-byte outputs during ODR updates. BDU (Block Data Update) function exists in both (CTRL3_C/CTRL3 bit 6) to prevent reads of partially-updated data but does not impose I2C timing constraints.

## Could not cite

None — all rows have valid page cites from both datasheets.

## Coverage

Compared LSM6DSO32 and LSM6DSV320X I2C interface sections (DSO32: sections 4.4.2, 5.1, 5.1.1, 5.2–5.3; DSV320X: sections 4.4.2, 5.1, 5.1.1–5.1.2, 5.2–5.3), register tables (CTRL3_C/CTRL4_C vs IF_CFG/PIN_CTRL/CTRL3), register map tables, electrical characteristics (VDDIO, VIH/VIL), and timing tables (I2C slave/target timing). Searched for and documented: 7-bit I2C address schemes and SDO/SA0–TA0 LSB selection, CS pin I2C/SPI mode selection, I2C speed modes (standard/fast/fast-mode-plus) and maximum SCL frequencies, I2C timing parameters (t_LOW, t_HIGH, t_SU;DAT, t_HD;DAT, rise/fall measurement points), multi-byte I2C read and IF_INC auto-increment default/control, I3C v1.0/v1.1 presence and enable status, I2C_disable and I2C_I3C_disable register locations and defaults, pull-up configuration bits (SDO_PU_EN, SDA_PU_EN, SHUB_PU_EN) and internal pull-up values, bus timing constraints (no explicit wait-between-write requirement found), VDDIO range per electrical characteristics. I3C bus speed parameters and detailed CCC command tables were examined but not compared in this delta (covered in separate I3C pass). No data found regarding per-transaction I2C timing waits or BDU-specific I2C read constraints beyond the standard protocol.
