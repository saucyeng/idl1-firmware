# IDL1 bench bring-up — phase 1 checklist

## Build and flash

ESP-IDF v6.0.1, target `esp32c6`. First flash is always USB (it lays down the
bootloader, partition table and first OTA slot).

```
idf.py set-target esp32c6          # once per fresh checkout
idf.py build
idf.py -p COMx flash monitor       # COMx = the XIAO's USB port; Ctrl+] quits
```

Rebooting re-runs the report: press RESET or `Ctrl+T Ctrl+R` in the monitor.

## What healthy looks like

About 3 s after boot (the report watches GPS traffic and the button in that window):

```
I bringup: ================ IDL1 BOOT SELF-TEST ================
I bringup: | PWR_EN       PASS   GPIO2 reads 1 (switched 3V3 rail enabled)
I bringup: | SD           PASS   mounted, 30436 MB total, 30401 MB free (OK)
I bringup: | IMU0 SPI     PASS   WHO_AM_I=0x73 LSM6DSV320X (not driven until phase 2)
I bringup: | POD F 0x6A   PASS   WHO_AM_I=0x73 LSM6DSV320X (not driven until phase 2)
I bringup: | POD R 0x6B   PASS   WHO_AM_I=0x73 LSM6DSV320X (not driven until phase 2)
I bringup: | TLA2528      PASS   ACK at 0x10, SYSTEM_STATUS=0x81, green LED on
I bringup: | BATTERY      PASS   pin 2650 mV (raw 2980) x1.5 -> Vbat 3975 mV
I bringup: | USER_BTN     PASS   GPIO6=1 at start (released); press seen in 3000 ms window: no
I bringup: | GPS          PASS   1460 bytes, 24 NMEA lines in 3000 ms (state NOFIX)
I bringup: ---------------- 9 PASS  0 FAIL  0 ABSENT ----------------
```

The same result goes to `idl0_debug.log` on the SD card as one `selftest ...`
line. The green board LED (D2) lights when the TLA2528 is up; red (D1) stays
off in phase 1. The XIAO's onboard LED blinks slowly whatever the TLA2528 does.
The IMUs are only detected in phase 1, so sessions record GPS only. That is
expected. The BLE status line shows `IMU: ABSENT`, or `IMU: ERROR` if a pod is
unplugged, because an enabled pod that does not answer counts as down.

To test the button, hold SW1 during the 3 s window. You should see
`press seen ... yes`. Holding it from reset gives FAIL (`LOW: held or shorted`).

## When a row is not PASS

| Row | Meaning / what to do |
|---|---|
| **PWR_EN FAIL**, or everything else FAIL at once | The switched rail is off. Check GPIO2 → U2 (TPS22919) ON pin and U2's output. Nothing but the MCU works without it. |
| **TLA2528 FAIL, no ACK at 0x10** | This is the expected failure on the boards as built. ADDR is shorted to GND through copper net tie **NT1**, a configuration TI does not document, so the address is unknown (decisions.md H2). The firmware probes one address only and never scans. The rest of boot carries on without the board LEDs or analog. Two ways out:<br>1. **Try a candidate:** edit `IDL1_I2C_ADDR_TLA` in `main/pins.h` (one line, try 0x10–0x17), rebuild, reflash and read the row again.<br>2. **Cut NT1** (knife through the copper between its pads). This gives the documented 0x10. It is not cleanly reversible. |
| TLA2528 FAIL, ACK but reserved-bit check fails | Something answered at that address but it is not behaving like a TLA2528 (SYSTEM_STATUS bit 7 should be 1, bits 4 and 1 should be 0). Check the raw byte. |
| **POD F / POD R ABSENT** | No ACK at that address. Check the M8 cable and the pod's JP1: front = JP1 **open** = 0x6A, rear = JP1 **closed** = 0x6B (decisions.md P7·H3). If both pods are jumpered the same way, one of them shows at the other's address. An unplugged pod is a normal configuration. |
| POD / IMU0 FAIL with an unexpected WHO_AM_I | A chip answered, but it is not an LSM6DSV320X (0x73). Note the raw value. |
| **IMU0 SPI FAIL**, WHO_AM_I 0x00 or 0xFF | Nothing is driving MISO. Check the rail, CS on GPIO21, and SPI 18/19/20. |
| **SD FAIL** | The card is missing, unformatted (FAT) or unreadable. CS is GPIO7 on IDL1. |
| **BATTERY ABSENT / FAIL** | ABSENT means under 0.5 V: no cell, or the divider is open. FAIL means outside 3.0–4.35 V: check the reading against a meter at BAT+ (Vbat = 1.5 × pin). |
| **GPS FAIL** | 0 bytes means no UART traffic. Check the rail and the module TX → GPIO17 link. Bytes with 0 NMEA lines means a baud or wiring problem. The module has 9600 baud factory defaults. |

## OTA

The firmware only accepts OTA images whose `project_name` is `idl1_firmware`.
An idl0-firmware image offered by the idl0 app gets a 400 error with
"image is for project idl0_firmware, this board runs idl1_firmware", and
nothing is written to flash (decisions.md F5).
