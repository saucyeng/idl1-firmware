# IDL1 Netlist Extraction (2026-08-31)

**Source:** `C:\Users\isaac\AppData\Local\Temp\claude\c--Users-isaac-Documents-Saucy-saucyeng\ca9d12a2-7d92-4ff1-ad84-e0b6b5171ee5\scratchpad\nets\IDL1.net`
**Export Date:** 2026-08-31T21:12:58
**Total Nets:** 64
**Total Components:** 58

## 1. XIAO ESP32-C6 Connected Nets

| Net Name | Component.Pin | XIAO Pad | GPIO | Notes |
|----------|---------------|----------|------|-------|
| +3V3_ALWAYS | ESP32C6.12 | - | - | C6.1; U2.1 |
| +3V3_ALWAYS | ESP32C6.23 | - | - | C6.1; U2.1 |
| /MCU/BAT_+ | ESP32C6.16 | - | - | J3.1; R2.1 |
| /MCU/BAT_SENSE | ESP32C6.22 | - | GPIO4 | C5.1; R2.2; R3.1 |
| /MCU/PWR_EN | ESP32C6.3 | D2 | GPIO2 | U2.3 |
| /MCU/USER_BTN | ESP32C6.21 | - | GPIO6 | SW1.1 |
| /MCU/WHEEL_F | ESP32C6.1 | D0 | GPIO0 | C12.1; R12.1; R14.2 |
| /MCU/WHEEL_R | ESP32C6.2 | D1 | GPIO1 | C15.1; R15.2; R16.1 |
| GND | ESP32C6.13 | - | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| GND | ESP32C6.15 | - | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| GND | ESP32C6.19 | - | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| GPS_RX_TO_MCU_TX | ESP32C6.7 | D6 | GPIO16 | U3.14 |
| GPS_TX_TO_MCU_RX | ESP32C6.8 | D7 | GPIO17 | U3.13 |
| IMU1_CS | ESP32C6.4 | D3 | GPIO21 | IMU1.12 |
| MISO | ESP32C6.10 | D9 | GPIO20 | IMU1.1; J1.7 |
| MOSI | ESP32C6.11 | D10 | GPIO18 | IMU1.14; J1.3 |
| SCL | ESP32C6.6 | D5 | GPIO23 | R11.2; R9.2; U4.13 |
| SDA | ESP32C6.5 | D4 | GPIO22 | R10.2; R8.2; U4.14 |
| SD_CS | ESP32C6.17 | - | GPIO7 | J1.2 |
| SPI_SCK | ESP32C6.9 | D8 | GPIO19 | IMU1.13; J1.5 |
| unconnected-(ESP32C6-EN-Pad20) | ESP32C6.20 | - | - | - |
| unconnected-(ESP32C6-GPIO5{slash}A5{slash}MTDO-Pad18) | ESP32C6.18 | - | GPIO5 | - |
| unconnected-(ESP32C6-GPIO9{slash}BOOT-Pad24) | ESP32C6.24 | - | GPIO9 | - |
| unconnected-(ESP32C6-VBUS-Pad14) | ESP32C6.14 | - | - | - |

## 2. Non-XIAO Connected Nets

| Net Name | Nodes |
|----------|-------|
| +3V3 | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U3.17; U3.2; U3.3; U4.10; U4.7 |
| /MCU/ANA_1 | C19.1; R18.1; U4.1 |
| /MCU/ANA_1_FIELD | J2.1; R18.2; U5.2; U5.9 |
| /MCU/ANA_2 | C20.1; R19.1; U4.2 |
| /MCU/ANA_2_FIELD | J2.2; R19.2; U5.1; U5.10 |
| /MCU/ANA_F | C13.1; R13.1; U4.4 |
| /MCU/ANA_F_FIELD | J5.6; R13.2; U5.4; U5.7 |
| /MCU/ANA_R | C16.1; R17.1; U4.3 |
| /MCU/ANA_R_FIELD | J4.6; R17.2; U6.4; U6.7 |
| /MCU/DECAP | C8.1; U4.8 |
| /MCU/LED1_A | D1.2; R1.2 |
| /MCU/LED1_DRV | R1.1; U4.5 |
| /MCU/LED2_A | D2.2; R5.1 |
| /MCU/LED2_DRV | R5.2; U4.6 |
| /MCU/QWIIC_3V3 | C18.1; F3.2; J6.2 |
| /MCU/SCL_FIELD | J4.4; J5.4; J6.4; R11.1; U6.2; U6.9 |
| /MCU/SDA_FIELD | J4.3; J5.3; J6.3; R10.1; U6.1; U6.10 |
| /MCU/WHEEL_F_FIELD | J5.5; R12.2; U5.5; U5.6 |
| /MCU/WHEEL_R_FIELD | J4.5; R16.2; U6.5; U6.6 |
| Net-(BT1-Pad??) | BT1.1; BT1.2 |
| Net-(U4-ADDR) | NT1.2; U4.11 |
| unconnected-(IMU1-INT1-Pad4) | IMU1.4 |
| unconnected-(IMU1-INT2-Pad9) | IMU1.9 |
| unconnected-(IMU1-OCS_AUX-Pad10) | IMU1.10 |
| unconnected-(IMU1-SCX-Pad3) | IMU1.3 |
| unconnected-(IMU1-SDO_AUX-Pad11) | IMU1.11 |
| unconnected-(IMU1-SDX-Pad2) | IMU1.2 |
| unconnected-(J1-DAT1-Pad8) | J1.8 |
| unconnected-(J1-DAT2-Pad1) | J1.1 |
| unconnected-(MH1-Pad1) | MH1.1 |
| unconnected-(MH2-Pad1) | MH2.1 |
| unconnected-(MH3-Pad1) | MH3.1 |
| unconnected-(MH4-Pad1) | MH4.1 |
| unconnected-(U2-N_C-Pad4) | U2.4 |
| unconnected-(U3-EXTINT-Pad19) | U3.19 |
| unconnected-(U3-RESET_N-Pad18) | U3.18 |
| unconnected-(U3-SAFEBOOT_N-Pad8) | U3.8 |
| unconnected-(U3-SCL-Pad12) | U3.12 |
| unconnected-(U3-SDA-Pad9) | U3.9 |
| unconnected-(U3-TIMEPULSE-Pad7) | U3.7 |
| unconnected-(U4-AIN0{slash}GPIO0-Pad15) | U4.15 |
| unconnected-(U4-AIN1{slash}GPIO1-Pad16) | U4.16 |
| unconnected-(U4-NC-Pad12) | U4.12 |

## 3. Component Table

| Ref | Value | Footprint | Sheet | Nets |
|-----|-------|-----------|-------|------|
| C11 | 1uF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | +3V3; GND |
| C12 | 10nF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | /MCU/WHEEL_F; GND |
| C13 | 100nF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | /MCU/ANA_F; GND |
| C14 | 1uF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | +3V3; GND |
| C15 | 10nF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | /MCU/WHEEL_R; GND |
| C16 | 100nF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | /MCU/ANA_R; GND |
| C17 | 1uF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | +3V3; GND |
| C18 | 1uF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | /MCU/QWIIC_3V3; GND |
| C19 | 100nF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | /MCU/ANA_1; GND |
| C2 | 100nF | Capacitor_SMD:C_0603_1608Metric | /Peripherals/ | +3V3; GND |
| C20 | 100nF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | /MCU/ANA_2; GND |
| C3 | 100nF | Capacitor_SMD:C_0603_1608Metric | /Peripherals/ | +3V3; GND |
| C4 | 100nF | Capacitor_SMD:C_0603_1608Metric | /Peripherals/ | +3V3; GND |
| C5 | 100nF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | /MCU/BAT_SENSE; GND |
| C6 | 100nF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | +3V3_ALWAYS; GND |
| C7 | 1uF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | +3V3; GND |
| C8 | 1uF | Capacitor_SMD:C_0603_1608Metric | /MCU/ | /MCU/DECAP; GND |
| D1 | LED_Red | LED_SMD:LED_0805_2012Metric | /MCU/ | /MCU/LED1_A; GND |
| D2 | LED_Green | LED_SMD:LED_0805_2012Metric | /MCU/ | /MCU/LED2_A; GND |
| D3 | ESD5B5.0ST1G | Diode_SMD:D_SOD-523 | /MCU/ | +3V3; GND |
| ESP32C6 | XIAO-ESP32-C6 | IDL1:XIAO-ESP32C6-SMD | /MCU/ | +3V3_ALWAYS; /MCU/BAT_+; /MCU/BAT_SENSE; /MCU/PWR_EN; /MCU/USER_BTN; /MCU/WHEEL_F; /MCU/WHEEL_R; GND; GPS_RX_TO_MCU_TX; GPS_TX_TO_MCU_RX; IMU1_CS; MISO; MOSI; SCL; SDA; SD_CS; SPI_SCK; unconnected-(ESP32C6-EN-Pad20); unconnected-(ESP32C6-GPIO5{slash}A5{slash}MTDO-Pad18); unconnected-(ESP32C6-GPIO9{slash}BOOT-Pad24); unconnected-(ESP32C6-VBUS-Pad14) |
| F3 | 100mA | Fuse:Fuse_0603_1608Metric | /MCU/ | +3V3; /MCU/QWIIC_3V3 |
| IMU1 | LSM6DSV320XTR | IDL1:XDCR_LSM6DSO32TR | /Peripherals/ | +3V3; GND; IMU1_CS; MISO; MOSI; SPI_SCK; unconnected-(IMU1-INT1-Pad4); unconnected-(IMU1-INT2-Pad9); unconnected-(IMU1-OCS_AUX-Pad10); unconnected-(IMU1-SCX-Pad3); unconnected-(IMU1-SDO_AUX-Pad11); unconnected-(IMU1-SDX-Pad2) |
| J1 | 502570-0893 | IDL1:CONN_microSD_Molex_502570-0893 | /Peripherals/ | +3V3; GND; MISO; MOSI; SD_CS; SPI_SCK; unconnected-(J1-DAT1-Pad8); unconnected-(J1-DAT2-Pad1) |
| J2 | Conn_01x03 | Connector_JST:JST_ZH_B2B-ZR_1x02_P1.50mm_Vertical | /MCU/ | /MCU/ANA_1_FIELD; /MCU/ANA_2_FIELD |
| J3 | S2B-PH-K-S | IDL1:JST_S2B-PH-K-S | /MCU/ | /MCU/BAT_+; GND |
| J4 | M8 | IDL1:CONN_CDM801-06A-MP-F810-67 | /MCU/ | +3V3; /MCU/ANA_R_FIELD; /MCU/SCL_FIELD; /MCU/SDA_FIELD; /MCU/WHEEL_R_FIELD; GND |
| J5 | M8 | IDL1:CONN_CDM801-06A-MP-F810-67 | /MCU/ | +3V3; /MCU/ANA_F_FIELD; /MCU/SCL_FIELD; /MCU/SDA_FIELD; /MCU/WHEEL_F_FIELD; GND |
| J6 | SM04B-SRSS-TB | Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal | /MCU/ | /MCU/QWIIC_3V3; /MCU/SCL_FIELD; /MCU/SDA_FIELD; GND |
| LP1 | MECH_LightPipe_VLP-350-F | IDL1:MECH_LightPipe_Bivar_VLP-350-F | /MCU/ | - |
| LP2 | MECH_LightPipe_VLP-350-F | IDL1:MECH_LightPipe_Bivar_VLP-350-F | /MCU/ | - |
| MH1 | M2.5 | MountingHole_2.7mm_M2.5_Pad_Via | /Peripherals/ | unconnected-(MH1-Pad1) |
| MH2 | M2.5 | MountingHole_2.7mm_M2.5_Pad_Via | /Peripherals/ | unconnected-(MH2-Pad1) |
| MH3 | M2.5 | MountingHole_2.7mm_M2.5_Pad_Via | /Peripherals/ | unconnected-(MH3-Pad1) |
| MH4 | M2.5 | MountingHole_2.7mm_M2.5_Pad_Via | /Peripherals/ | unconnected-(MH4-Pad1) |
| NT1 | NetTie_2 | IDL1:NetTie-2_SMD_Pad0.2mm | /MCU/ | GND; Net-(U4-ADDR) |
| R1 | 330R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/LED1_A; /MCU/LED1_DRV |
| R10 | 33R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/SDA_FIELD; SDA |
| R11 | 33R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/SCL_FIELD; SCL |
| R12 | 330R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/WHEEL_F; /MCU/WHEEL_F_FIELD |
| R13 | 33R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/ANA_F; /MCU/ANA_F_FIELD |
| R14 | 4k7 | Resistor_SMD:R_0603_1608Metric | /MCU/ | +3V3; /MCU/WHEEL_F |
| R15 | 4k7 | Resistor_SMD:R_0603_1608Metric | /MCU/ | +3V3; /MCU/WHEEL_R |
| R16 | 330R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/WHEEL_R; /MCU/WHEEL_R_FIELD |
| R17 | 33R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/ANA_R; /MCU/ANA_R_FIELD |
| R18 | 33R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/ANA_1; /MCU/ANA_1_FIELD |
| R19 | 33R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/ANA_2; /MCU/ANA_2_FIELD |
| R2 | 1M | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/BAT_+; /MCU/BAT_SENSE |
| R3 | 2M | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/BAT_SENSE; GND |
| R5 | 330R | Resistor_SMD:R_0603_1608Metric | /MCU/ | /MCU/LED2_A; /MCU/LED2_DRV |
| R8 | 4k7 | Resistor_SMD:R_0603_1608Metric | /MCU/ | +3V3; SDA |
| R9 | 4k7 | Resistor_SMD:R_0603_1608Metric | /MCU/ | +3V3; SCL |
| SW1 | SW_Push | IDL1:SW_Panasonic_EVQP2 | /MCU/ | /MCU/USER_BTN; GND |
| U2 | TPS22919DCKR | IDL1:DCK0006A_N | /MCU/ | +3V3; +3V3_ALWAYS; /MCU/PWR_EN; GND; unconnected-(U2-N_C-Pad4) |
| U3 | SAM-M10Q-00B | IDL1:XCVR_SAM-M10Q-00B | /Peripherals/ | +3V3; GND; GPS_RX_TO_MCU_TX; GPS_TX_TO_MCU_RX; unconnected-(U3-EXTINT-Pad19); unconnected-(U3-RESET_N-Pad18); unconnected-(U3-SAFEBOOT_N-Pad8); unconnected-(U3-SCL-Pad12); unconnected-(U3-SDA-Pad9); unconnected-(U3-TIMEPULSE-Pad7) |
| U4 | TLA2528IRTET | IDL1:WQFN16_RTE_TEX | /MCU/ | +3V3; /MCU/ANA_1; /MCU/ANA_2; /MCU/ANA_F; /MCU/ANA_R; /MCU/DECAP; /MCU/LED1_DRV; /MCU/LED2_DRV; GND; Net-(U4-ADDR); SCL; SDA; unconnected-(U4-AIN0{slash}GPIO0-Pad15); unconnected-(U4-AIN1{slash}GPIO1-Pad16); unconnected-(U4-NC-Pad12) |
| U5 | TPD4E05U06DQAR | IDL1:DQA0010A | /MCU/ | /MCU/ANA_1_FIELD; /MCU/ANA_2_FIELD; /MCU/ANA_F_FIELD; /MCU/WHEEL_F_FIELD; GND |
| U6 | TPD4E05U06DQAR | IDL1:DQA0010A | /MCU/ | /MCU/ANA_R_FIELD; /MCU/SCL_FIELD; /MCU/SDA_FIELD; /MCU/WHEEL_R_FIELD; GND |

## 4. Detailed Component Analysis

### a. LEDs (D1, D2)
| LED | Net | Nodes |
|-----|-----|-------|
| D1 | /MCU/LED1_A | D1.2; R1.2 |
| D1 | GND | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| D2 | /MCU/LED2_A | D2.2; R5.1 |
| D2 | GND | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |

### b. Button (SW1)
| Net | GPIO | Other Nodes |
|-----|------|-------------|
| /MCU/USER_BTN | GPIO6 | ESP32C6.21 |
| GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |

### c. GNSS (U3)
| Comp | Pin | Net | GPIO | Other Nodes |
|------|-----|-----|------|-------------|
| U3 | 17 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U4.10; U4.7 |
| U3 | 2 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U4.10; U4.7 |
| U3 | 3 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U4.10; U4.7 |
| U3 | 1 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U3 | 10 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U3 | 11 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U3 | 15 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U3 | 16 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U3 | 20 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U3 | 4 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U3 | 5 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U3 | 6 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U3 | 14 | GPS_RX_TO_MCU_TX | GPIO16 | ESP32C6.7 |
| U3 | 13 | GPS_TX_TO_MCU_RX | GPIO17 | ESP32C6.8 |
| U3 | 19 | unconnected-(U3-EXTINT-Pad19) | - |  |
| U3 | 18 | unconnected-(U3-RESET_N-Pad18) | - |  |
| U3 | 8 | unconnected-(U3-SAFEBOOT_N-Pad8) | - |  |
| U3 | 12 | unconnected-(U3-SCL-Pad12) | - |  |
| U3 | 9 | unconnected-(U3-SDA-Pad9) | - |  |
| U3 | 7 | unconnected-(U3-TIMEPULSE-Pad7) | - |  |

### d. IMU (IMU1)
| Comp | Pin | Net | GPIO | Other Nodes |
|------|-----|-----|------|-------------|
| IMU1 | 5 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U3.17; U3.2; U3.3; U4.10; U4.7 |
| IMU1 | 8 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U3.17; U3.2; U3.3; U4.10; U4.7 |
| IMU1 | 6 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| IMU1 | 7 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| IMU1 | 12 | IMU1_CS | GPIO21 | ESP32C6.4 |
| IMU1 | 1 | MISO | GPIO20 | ESP32C6.10; J1.7 |
| IMU1 | 14 | MOSI | GPIO18 | ESP32C6.11; J1.3 |
| IMU1 | 13 | SPI_SCK | GPIO19 | ESP32C6.9; J1.5 |
| IMU1 | 4 | unconnected-(IMU1-INT1-Pad4) | - |  |
| IMU1 | 9 | unconnected-(IMU1-INT2-Pad9) | - |  |
| IMU1 | 10 | unconnected-(IMU1-OCS_AUX-Pad10) | - |  |
| IMU1 | 3 | unconnected-(IMU1-SCX-Pad3) | - |  |
| IMU1 | 11 | unconnected-(IMU1-SDO_AUX-Pad11) | - |  |
| IMU1 | 2 | unconnected-(IMU1-SDX-Pad2) | - |  |

### e. microSD (J1)
| Conn | Pin | Net | GPIO | Other Nodes |
|------|-----|-----|------|-------------|
| J1 | 4 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U3.17; U3.2; U3.3; U4.10; U4.7 |
| J1 | 10 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| J1 | 6 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| J1 | 9 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| J1 | 7 | MISO | GPIO20 | ESP32C6.10; IMU1.1 |
| J1 | 3 | MOSI | GPIO18 | ESP32C6.11; IMU1.14 |
| J1 | 2 | SD_CS | GPIO7 | ESP32C6.17 |
| J1 | 5 | SPI_SCK | GPIO19 | ESP32C6.9; IMU1.13 |
| J1 | 8 | unconnected-(J1-DAT1-Pad8) | - |  |
| J1 | 1 | unconnected-(J1-DAT2-Pad1) | - |  |

### f. ADC (U4)
| Comp | Pin | Net | GPIO | Other Nodes |
|------|-----|-----|------|-------------|
| U4 | 10 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U3.17; U3.2; U3.3 |
| U4 | 7 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U3.17; U3.2; U3.3 |
| U4 | 1 | /MCU/ANA_1 | - | C19.1; R18.1 |
| U4 | 2 | /MCU/ANA_2 | - | C20.1; R19.1 |
| U4 | 4 | /MCU/ANA_F | - | C13.1; R13.1 |
| U4 | 3 | /MCU/ANA_R | - | C16.1; R17.1 |
| U4 | 8 | /MCU/DECAP | - | C8.1 |
| U4 | 5 | /MCU/LED1_DRV | - | R1.1 |
| U4 | 6 | /MCU/LED2_DRV | - | R5.2 |
| U4 | 17 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U5.3; U5.8; U6.3; U6.8 |
| U4 | 9 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U5.3; U5.8; U6.3; U6.8 |
| U4 | 11 | Net-(U4-ADDR) | - | NT1.2 |
| U4 | 13 | SCL | GPIO23 | ESP32C6.6; R11.2; R9.2 |
| U4 | 14 | SDA | GPIO22 | ESP32C6.5; R10.2; R8.2 |
| U4 | 15 | unconnected-(U4-AIN0{slash}GPIO0-Pad15) | - |  |
| U4 | 16 | unconnected-(U4-AIN1{slash}GPIO1-Pad16) | - |  |
| U4 | 12 | unconnected-(U4-NC-Pad12) | - |  |

### g. Pod Connectors (J4, J5)
| Conn | Pin | Net | GPIO | Other Nodes |
|------|-----|-----|------|-------------|
| J4 | 2 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U3.17; U3.2; U3.3; U4.10; U4.7 |
| J4 | 6 | /MCU/ANA_R_FIELD | - | R17.2; U6.4; U6.7 |
| J4 | 4 | /MCU/SCL_FIELD | - | J5.4; J6.4; R11.1; U6.2; U6.9 |
| J4 | 3 | /MCU/SDA_FIELD | - | J5.3; J6.3; R10.1; U6.1; U6.10 |
| J4 | 5 | /MCU/WHEEL_R_FIELD | - | R16.2; U6.5; U6.6 |
| J4 | 1 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| J5 | 2 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J4.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U3.17; U3.2; U3.3; U4.10; U4.7 |
| J5 | 6 | /MCU/ANA_F_FIELD | - | R13.2; U5.4; U5.7 |
| J5 | 4 | /MCU/SCL_FIELD | - | J4.4; J6.4; R11.1; U6.2; U6.9 |
| J5 | 3 | /MCU/SDA_FIELD | - | J4.3; J6.3; R10.1; U6.1; U6.10 |
| J5 | 5 | /MCU/WHEEL_F_FIELD | - | R12.2; U5.5; U5.6 |
| J5 | 1 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |

### h. Other Connectors (J1, J2, J3, J6)
| Conn | Pin | Net | GPIO | Other Nodes |
|------|-----|-----|------|-------------|
| J1 | 4 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U2.5; U2.6; U3.17; U3.2; U3.3; U4.10; U4.7 |
| J1 | 10 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| J1 | 6 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| J1 | 9 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| J1 | 7 | MISO | GPIO20 | ESP32C6.10; IMU1.1 |
| J1 | 3 | MOSI | GPIO18 | ESP32C6.11; IMU1.14 |
| J1 | 2 | SD_CS | GPIO7 | ESP32C6.17 |
| J1 | 5 | SPI_SCK | GPIO19 | ESP32C6.9; IMU1.13 |
| J1 | 8 | unconnected-(J1-DAT1-Pad8) | - |  |
| J1 | 1 | unconnected-(J1-DAT2-Pad1) | - |  |
| J2 | 1 | /MCU/ANA_1_FIELD | - | R18.2; U5.2; U5.9 |
| J2 | 2 | /MCU/ANA_2_FIELD | - | R19.2; U5.1; U5.10 |
| J3 | 1 | /MCU/BAT_+ | - | ESP32C6.16; R2.1 |
| J3 | 2 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| J6 | 2 | /MCU/QWIIC_3V3 | - | C18.1; F3.2 |
| J6 | 4 | /MCU/SCL_FIELD | - | J4.4; J5.4; R11.1; U6.2; U6.9 |
| J6 | 3 | /MCU/SDA_FIELD | - | J4.3; J5.3; R10.1; U6.1; U6.10 |
| J6 | 1 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; NT1.1; R3.2; SW1.2; SW1.3; U2.2; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |

### i. Power (U2)
| Comp | Pin | Net | GPIO | Other Nodes |
|------|-----|-----|------|-------------|
| U2 | 5 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U3.17; U3.2; U3.3; U4.10; U4.7 |
| U2 | 6 | +3V3 | - | C11.1; C14.1; C17.1; C2.1; C3.1; C4.2; C7.1; D3.1; F3.1; IMU1.5; IMU1.8; J1.4; J4.2; J5.2; R14.1; R15.1; R8.1; R9.1; U3.17; U3.2; U3.3; U4.10; U4.7 |
| U2 | 1 | +3V3_ALWAYS | - | C6.1; ESP32C6.12; ESP32C6.23 |
| U2 | 3 | /MCU/PWR_EN | GPIO2 | ESP32C6.3 |
| U2 | 2 | GND | - | C11.2; C12.2; C13.2; C14.2; C15.2; C16.2; C17.2; C18.2; C19.2; C2.2; C20.2; C3.2; C4.1; C5.2; C6.2; C7.2; C8.2; D1.1; D2.1; D3.2; ESP32C6.13; ESP32C6.15; ESP32C6.19; IMU1.6; IMU1.7; J1.10; J1.6; J1.9; J3.2; J4.1; J5.1; J6.1; NT1.1; R3.2; SW1.2; SW1.3; U3.1; U3.10; U3.11; U3.15; U3.16; U3.20; U3.4; U3.5; U3.6; U4.17; U4.9; U5.3; U5.8; U6.3; U6.8 |
| U2 | 4 | unconnected-(U2-N_C-Pad4) | - |  |

### j-m. Additional Analysis
See netlist and component tables above for detailed tracing.

## 5. Reproduction Info

- **Netlist File:** `C:\Users\isaac\AppData\Local\Temp\claude\c--Users-isaac-Documents-Saucy-saucyeng\ca9d12a2-7d92-4ff1-ad84-e0b6b5171ee5\scratchpad\nets\IDL1.net`
- **Export Date:** 2026-08-31T21:12:58
- **Parser:** parse_idl1.py (KiCad 10.0 s-expression parser)
- **Generated:** 2026-08-31T21:19:03.123611
- **Net Count:** 64
- **Component Count:** 58