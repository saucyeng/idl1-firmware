# TLA2528 Register Map and Protocol Reference

## Register map

| address (hex) | name | reset value | one-line description | cite |
|---|---|---|---|---|
| 0x0 | SYSTEM_STATUS | 0x80 | System status register with sequencer, I2C speed, OSR, and brown-out indicators. | tla2528.md:752 |
| 0x1 | GENERAL_CFG | 0x0 | General configuration register for conversion start, channel reset, calibration, and software reset. | tla2528.md:753 |
| 0x2 | DATA_CFG | 0x0 | Data configuration register for fixed pattern mode and channel ID append control. | tla2528.md:754 |
| 0x3 | OSR_CFG | 0x0 | Oversampling ratio configuration register. | tla2528.md:755 |
| 0x4 | OPMODE_CFG | 0x0 | Operating mode configuration register for oscillator and clock divider selection. | tla2528.md:756 |
| 0x5 | PIN_CFG | 0x0 | Pin configuration register to select channels as analog input or GPIO. | tla2528.md:757 |
| 0x7 | GPIO_CFG | 0x0 | GPIO configuration register to select GPIO direction (input or output). | tla2528.md:758 |
| 0x9 | GPO_DRIVE_CFG | 0x0 | GPIO output drive configuration register (open-drain or push-pull). | tla2528.md:759 |
| 0xB | GPO_VALUE | 0x0 | GPIO output value register to control logic level on digital outputs. | tla2528.md:760 |
| 0xD | GPI_VALUE | 0x0 | GPIO input value register to read logic level on digital inputs. | tla2528.md:761 |
| 0x10 | SEQUENCE_CFG | 0x0 | Sequencer configuration register for auto-sequence mode control. | tla2528.md:762 |
| 0x11 | CHANNEL_SEL | 0x0 | Channel selection register for manual mode channel ID. | tla2528.md:763 |
| 0x12 | AUTO_SEQ_CH_SEL | 0x0 | Auto-sequence channel selection register to enable channels in scanning. | tla2528.md:764 |

## Bit fields

### SYSTEM_STATUS Register (Address = 0x0) [reset = 0x80]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7 | RSVD | R | 1b | This bit must read 1b. | tla2528.md:799 |
| 6 | SEQ_STATUS | R | 0b | Sequencer Status 0b = Sequence stopped 1b = Sequence in progress | tla2528.md:800 |
| 5 | I2C_SPEED | R | 0b | I2C high-speed status 0b = Device is not in high speed mode 1b = Device is in high speed mode | tla2528.md:801 |
| 4 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:802 |
| 3 | OSR_DONE | R/W | 0b | OSR status. Clear this bit by writing 1b to this bit. 0b = OSR in progress; data not ready. 1b = OSR complete; data ready. | tla2528.md:803 |
| 2 | CRC_ERR_FUSE | R | 0b | Device fuse CRC check status. To re-evaluate this bit, software reset the device or power cycle AVDD. 0b = Configuration is good. 1b = Device configuration not loaded correctly. | tla2528.md:804 |
| 1 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:805 |
| 0 | BOR | R/W | 0b | Brown out reset indicator. This bit is set if brown out condition occurs or device is power cycled. Write 1 to this bit to clear the flag. 0b = No brown out from last time this bit was cleared. 1b = Brown out condition detected or device power cycled. | tla2528.md:806 |

### GENERAL_CFG Register (Address = 0x1) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-4 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:825 |
| 3 | CNVST | W | 0b | Intiate start of conversion. Readback of this bit will return 0. 0b = Normal operation. 1b = Initiate start of conversion. | tla2528.md:826 |
| 2 | CH_RST | R/W | 0b | Force all channels to be analog inputs. 0b = Normal operation. 1b = All channels will be set as analog inputs irrespective of configuration in other registers. | tla2528.md:827 |
| 1 | CAL | R/W | 0b | Calibrate ADC offset. 0b = Normal operation. 1b = ADC offset will be calibrated. After calibration is complete, this bit will be set to 0. | tla2528.md:828 |
| 0 | RST | W | 0b | Software reset all registers to default values. 0b = Normal operation. 1b = Device will be reset. After reset is complete, this bit will be set to 0. | tla2528.md:838 |

### DATA_CFG Register (Address = 0x2) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7 | FIX_PAT | R/W | 0b | Device outputs fixed data bits. Helpful for debugging device communication. 0b = Normal operation. 1b = Device outputs a fixed code 0xA5A repeatitively when reading ADC data. | tla2528.md:859 |
| 6 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:860 |
| 5-4 | APPEND_STATUS[1:0] | R/W | 0b | Append 4-bit channel ID to output data. 0b = Channel ID is not appended to ADC data. 1b = Channel ID is appended to ADC data. | tla2528.md:861 |
| 3-0 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:862 |

### OSR_CFG Register (Address = 0x3) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-3 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:879 |
| 2-0 | OSR[2:0] | R/W | 0b | Selects the oversampling ratio for ADC conversion result. 0b = OSR = 0. 1b = OSR = 2. 10b = OSR = 4. 11b = OSR = 8. 100b = OSR = 16. 101b = OSR = 32. 110b = OSR = 64. 111b = OSR = 128. | tla2528.md:880 |

### OPMODE_CFG Register (Address = 0x4) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-5 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:899 |
| 4 | OSC_SEL | R/W | 0b | Selects the oscillator for internal timing generation. 0b = High speed oscillator. 1b = Low power oscillator. | tla2528.md:900 |
| 3-0 | CLK_DIV[3:0] | R/W | 0b | Sampling speed control. Refer to section on Oscillator and Timing Control for details. | tla2528.md:901 |

### PIN_CFG Register (Address = 0x5) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-0 | PIN_CFG[7:0] | R/W | 0b | Configure device channels CH7 through CH0 as analog input or GPIO. 0b = Channel is configured as analog input. 1b = Channel is configured as GPIO. | tla2528.md:920 |

### GPIO_CFG Register (Address = 0x7) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-0 | GPIO_CFG[7:0] | R/W | 0b | Configure GPIO7 through GPIO0 as either digital input or digital output. 0b = GPIO is digital input. 1b = GPIO is digital output. | tla2528.md:939 |

### GPO_DRIVE_CFG Register (Address = 0x9) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-0 | GPO_DRIVE_CFG[7:0] | R/W | 0b | Configure digital outputs GPO7 through GPO0 as open-drain or push-pull output. 0b = Digital output is open-drain. Connect external pullup. 1b = Digital output is push-pull. | tla2528.md:962 |

### GPO_VALUE Register (Address = 0xB) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-0 | GPO_VALUE[7:0] | R/W | 0b | Logic level to be set on digital outputs GPO[7:0]. 0b = Digital output set to logic 0. 1b = Digital output set to logic 1. | tla2528.md:981 |

### GPI_VALUE Register (Address = 0xD) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-0 | GPI_VALUE[7:0] | R | 0b | Readback the logic level on digital input. 0b = Digital input is at logic 0. 1b = Digital input is at logic 1. | tla2528.md:1000 |

### SEQUENCE_CFG Register (Address = 0x10) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-5 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:1019 |
| 4 | SEQ_START | R/W | 0b | Sequence start control when using auto sequence mode. 0b = Stop auto sequencing. 1b = Start auto sequencing from first enabled analog input channel starting from channel ID = 0 (ascending order). | tla2528.md:1020 |
| 3-2 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:1021 |
| 1-0 | SEQ_MODE[1:0] | R/W | 0b | Selects the mode of scanning analog input channels. 0b = Manual sequence mode. 1b = Auto sequence mode. 10b = Reserved. 11b = Reserved. | tla2528.md:1022 |

### CHANNEL_SEL Register (Address = 0x11) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-4 | RESERVED | R | 0b | Reserved. Reads return 0b. | tla2528.md:1041 |
| 3-0 | MANUAL_CHID[3:0] | R/W | 0b | In manual mode, this field contains the 4-bit channel ID of the analog input channel for next ADC conversion. For valid ADC data, the channel ID must not be configured as GPIO. 0b = CH0 1b = CH1 10b = CH2 11b = CH3 100b = CH4 101b = CH5 110b = CH6 111b = CH7 1000b = Reserved. | tla2528.md:1042 |

### AUTO_SEQ_CH_SEL Register (Address = 0x12) [reset = 0x0]

| bit | field | type | reset | description | cite |
|---|---|---|---|---|---|
| 7-0 | AUTO_SEQ_CH_SEL[7:0] | R/W | 0b | Enable analog input channels AIN7 through AIN0 in auto sequencing mode. 0b = Analog input channel is not enabled in scanning sequence. 1b = Analog input channel is enabled in scanning sequence. | tla2528.md:1065 |

## I2C protocol

### I2C Frame Acronyms

| SYMBOL | DESCRIPTION |
|---|---|
| S | Start condition for the I2C frame |
| Sr | Restart condition for the I2C frame |
| P | Stop condition for the I2C frame |
| A | ACK (low) |
| N | NACK (high) |
| R | Read bit (high) |
| W | Write bit (low) |

(tla2528.md:605-616, tla2528.tables.json:17)

### Opcodes for Commands

| OPCODE | COMMAND DESCRIPTION |
|---|---|
| 0001 0000b (0x10) | Single register read |
| 0000 1000b (0x08) | Single register write |
| 0001 1000b (0x18) | Set bit |
| 0010 0000b (0x20) | Clear bit |
| 0011 0000b (0x30) | Reading a continuous block of registers |
| 0010 1000b (0x28) | Writing a continuous block of registers |

(tla2528.md:617-626, tla2528.tables.json:18)

### Single Register Read Frame Format

To read a single register from the device, the I2C master must provide an I2C command with three frames to set the register address for reading data. After this command is provided, the I2C master must provide another I2C frame containing the device address and the read bit. After this frame, the device provides the register data. The device provides the same register data even if the host provides more clocks. To end the register read command, the master must provide a STOP or a RESTART condition in the I2C frame.

Frame sequence: S [7-bit Slave Address W] A [0001 0000b] A [Register Address] A P/Sr S [7-bit Slave Address R] A [Register Data] A P/Sr

(tla2528.md:632-636)

### Single Register Write Frame Format

To write a single register from the device, the I2C master must provide an I2C command with four frames. The register address is the address of the register that must be written and the register data is the value that must be written. To end the register write command, the master must provide a STOP or a RESTART condition in the I2C frame.

Frame sequence: S [7-bit Slave Address W] A [0000 1000b] A [Register Address] A [Register Data] A P/Sr

(tla2528.md:688-691)

### Reading Conversion Data - Manual Mode

Provide an I2C start or restart frame to initiate a conversion. ADC data can be read in subsequent I2C frames. The number of I2C frames required to read conversion data depends on the output data frame size. A new conversion is initiated on the ninth falling edge of SCL (ACK bit) when the last byte of output data is read.

(tla2528.md:582-583)

### Reading Conversion Data - Auto-Sequence Mode

In auto-sequence mode, the internal channel sequencer switches the multiplexer to the next analog input channel after every conversion. The desired analog input channels can be configured for sequencing in the AUTO_SEQ_CH_SEL register. To enable the channel sequencer, set SEQ_START to 1b. After every conversion, the channel sequencer switches the multiplexer to the next analog input in ascending order. To stop the channel sequencer from selecting channels, set SEQ_START to 0b.

(tla2528.md:595)

## Conversion data format

Two 8-bit I2C packets are required to read the ADC conversion result (frame A).

Two 8-bit I2C packets are required to read the averaged conversion result (frame B).

The 4-bit channel ID can be appended to the 12-bit ADC result by configuring the APPEND_STATUS field in the DATA_CFG register (APPEND_STATUS[1:0]). When the channel ID is appended to the output data, the frame format changes to include the channel identifier.

Note: The exact bit-packing layout (MSB/LSB positioning and bit alignment) for the two data bytes with and without averaging is shown in Figure 25 (Data Frames for Reading Data) which is not extracted to text format in the datasheet extraction.

(tla2528.md:511-523)

The 12-bit ADC result represents the analog input voltage from 0 to AVDD. The conversion codes are:
- ≤1 LSB: NFSC (Negative full-scale code) = 000h
- 1 LSB to 2 LSBs: NFSC + 1 = 001h
- (AVDD / 2) to (AVDD / 2) + 1 LSB: MC (Mid code) = 800h
- (AVDD / 2) + 1 LSB to (AVDD / 2) + 2 LSB: MC + 1 = 801h
- ≥ AVDD – 1 LSB: PFSC (Positive full-scale code) = FFFh

(tla2528.md:412-417)

## Timing

### I2C Timing Requirements

| Parameter | Condition | Standard/Fast/Fast Mode Plus MIN | Standard/Fast/Fast Mode Plus MAX | High-Speed MIN | High-Speed MAX | Unit |
|---|---|---|---|---|---|---|
| fSCL | SCL clock frequency | — | 1 | — | 3.4 | MHz |
| tSUSTA | START condition setup time for repeated start | 260 | — | 160 | — | ns |
| tHDSTA | Start condition hold time | 260 | — | 160 | — | ns |
| tLOW | Clock low period | 500 | — | 160 | — | ns |
| tHIGH | Clock high period | 260 | — | 60 | — | ns |
| tSUDAT | Data in setup time | 50 | — | 10 | — | ns |
| tHDDAT | Data in hold time | 0 | — | 0 | — | ns |
| tR | SCL rise time | — | 120 | — | 80 | ns |
| tF | SCL fall time | 120 | — | — | 80 | ns |
| tSUSTO | STOP condition hold time | 260 | — | 60 | — | ns |
| tBUF | Bus free time before new transmission | 500 | — | 300 | — | ns |

(tla2528.md:251-267)

### I2C Switching Characteristics

| Parameter | Description | Standard/Fast/Fast Mode Plus MIN | Standard/Fast/Fast Mode Plus MAX | High-Speed MODE | Unit |
|---|---|---|---|---|---|
| tVDDATA | SCL low to SDA data out valid | 450 | — | 200 | ns |
| tVDACK | SCL low to SDA acknowledge time | 450 | — | 200 | ns |
| tSTRETCH | Clock stretch time in one-shot conversion mode | 1400 | — | 1000 | ns |
| tSP | Noise suppression time constant on SDA and SCL | 50 | — | 10 | ns |

(tla2528.md:277-286)

### Conversion and Power-up Timing

| Parameter | Description | Condition | MAX | Unit |
|---|---|---|---|---|
| tCONV | ADC conversion time | — | tSTRETCH | ns |
| tACQ | Acquisition time | — | 300 | ns |
| tPU | Power-up time for device | AVDD ≥ 2.35 V | 5 | ms |
| tRST | Delay time; RST bit = 1b to device reset complete | — | 5 | ms |

(tla2528.md:269-307)

**Clock Stretching:** The device uses SCL clock stretching during conversion. The tSTRETCH parameter defines the maximum clock stretch time, which is independent of OSC_SEL and CLK_DIV[3:0] configuration. During averaging operations, SCL is stretched by the device after the start of conversions until the averaging operation is complete.

(tla2528.md:462, 509)

## Address selection

| R1 Resistor | R2 Resistor | 7-bit I2C Address (binary) | 7-bit I2C Address (hex) |
|---|---|---|---|
| 0 Ω | DNP | 001 0111b | 17h |
| 11 kΩ | DNP | 001 0110b | 16h |
| 33 kΩ | DNP | 001 0101b | 15h |
| 100 kΩ | DNP | 001 0100b | 14h |
| DNP | DNP | 001 0000b | 10h |
| DNP | 11 kΩ | 001 0001b | 11h |
| DNP | 33 kΩ | 001 0010b | 12h |
| DNP | 100 kΩ | 001 0011b | 13h |

DNP = Do not populate.

The I2C address for the device is determined by connecting external resistors on the ADDR pin. The device address is determined at power-up based on the resistor values. The device retains this address until the next power-up event, until the next device reset, or until the device receives a command to program its own address. Figure 23 shows a connection diagram for the ADDR pin.

Tolerance for R1, R2 ≤ ±5%.

(tla2528.md:425-450, tla2528.tables.json:2)

## Could not find

1. Detailed bit-packing layout for the 12-bit conversion result in the two I2C data bytes (from/to LSB/MSB ordering). The specific information is in Figure 25 (Data Frames for Reading Data) which appears as an image in the datasheet and is not extracted to text in the provided markdown extraction.

2. Specific I2C sequence diagrams showing exact timing relationships and bus state transitions for command/data frames. These are shown in Figures 29-32 which appear as images in the datasheet.

3. Sampling rate lookup table for CLK_DIV[3:0] settings and their corresponding frequencies. Referenced as "Refer to section on Oscillator and Timing Control for details" in OPMODE_CFG but the detailed table is not clearly extracted to the markdown text.
