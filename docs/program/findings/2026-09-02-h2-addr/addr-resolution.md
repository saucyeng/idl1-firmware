# IDL1 TLA2528 ADDR Pin Resolution

**Date**: 2026-09-02
**Device**: TI TLA2528IRTET (U4)  
**Pin**: 11 (ADDR)  
**Project**: idl1-hardware / IDL1.kicad_sch

## Findings

### Net Tracing

From netlist export (IDL1_netlist.xml):

1. **U4 pin 11 connection (Line 6977-6980)**:
   - U4 pin 11 (ADDR) is on net: `Net-(U4-ADDR)` (code 35)
   - Netlist entry:
     ```
     (ref "U4")
     (pin "11")
     (pinfunction "ADDR_11")
     (pintype "input")
     ```

2. **NT1 pin 2 connection (Line 6971-6975)**:
   - NT1 pin 2 is on the same net as U4 ADDR
   - Netlist entry:
     ```
     (ref "NT1")
     (pin "2")
     (pinfunction "2_2")
     (pintype "passive")
     ```

3. **NT1 pin 1 connection (Line 6735-6738)**:
   - NT1 pin 1 is on net: `GND` (code 28)
   - Netlist entry (within GND net definition at line 6555-6557):
     ```
     (name "GND")
     (class "Ground,Analog,Default")
     ```
   - NT1 pin 1 node:
     ```
     (ref "NT1")
     (pin "1")
     (pinfunction "1_1")
     (pintype "passive")
     ```

### Conclusion

**ADDR is connected to GND through a 0-ohm net tie (NT1).**

- NT1 bridges the ADDR pin to the GND net
- This represents R1 = DNP, R2 = 0 ohm in TI's address selection terminology
- This configuration is **outside** TI's documented Address Selection Table (Table 2)

### TI Address Table Reference

Per TI datasheet Table 2, valid configurations are:

| R1 Value | R2 Value | Address |
|----------|----------|---------|
| 0 ohm    | DNP      | 0x17    |
| 11k      | DNP      | 0x16    |
| 33k      | DNP      | 0x15    |
| 100k     | DNP      | 0x14    |
| DNP      | DNP      | 0x10    |
| DNP      | 11k      | 0x11    |
| DNP      | 33k      | 0x12    |
| DNP      | 100k     | 0x13    |

**No entry exists for R2 = 0 ohm (direct to GND without resistor).**

### I2C Address

**Indeterminate** per TI's documented configurations. The board violates TI's address selection design. The device will likely not respond to any address in the standard table, or may respond unexpectedly due to an undocumented configuration.

## Contradiction Resolution

- **Run 1 (correct)**: ADDR ties to GND
- **Run 2 (incorrect)**: Reported address 0x17 (which would require ADDR tied to DECAP via 0-ohm resistor)

The netlist definitively shows ADDR is connected to GND, not DECAP.
