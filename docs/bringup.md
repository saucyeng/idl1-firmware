# IDL1 bench bring-up checklist (phases 1 + 2)

## Build and flash

ESP-IDF v6.0.1, target `esp32c6`. First flash is always USB (it lays down the
bootloader, partition table and first OTA slot).

```
idf.py set-target esp32c6          # once per fresh checkout
idf.py build
idf.py -p COMx flash monitor       # COMx = the XIAO's USB port; Ctrl+] quits
```

The partition table has two 1920 KB OTA slots (IDL0 had 1600 KB). Only a USB
flash writes the table, never an OTA, and every IDL1 board gets its first
flash over USB anyway, so the change costs nothing.

Rebooting re-runs the report: press RESET or `Ctrl+T Ctrl+R` in the monitor.

## What healthy looks like

About 3 s after boot (the report watches GPS traffic and the button in that window):

```
I bringup: ================ IDL1 BOOT SELF-TEST ================
I bringup: | PWR_EN       PASS   GPIO2 reads 1 (switched 3V3 rail enabled)
I bringup: | SD           PASS   mounted, 30436 MB total, 30401 MB free (OK)
I bringup: | IMU0 SPI     PASS   WHO_AM_I=0x73 LSM6DSV320X, configured and running
I bringup: | POD F 0x6A   PASS   WHO_AM_I=0x73 LSM6DSV320X, configured and running
I bringup: | POD R 0x6B   PASS   WHO_AM_I=0x73 LSM6DSV320X, configured and running
I bringup: | TLA2528      PASS   ACK at 0x10, SYSTEM_STATUS=0x81, green LED on
I bringup: | BATTERY      PASS   pin 2650 mV (raw 2980) x1.5 -> Vbat 3975 mV
I bringup: | USER_BTN     PASS   GPIO6=1 at start (released); press seen in 3000 ms window: no
I bringup: | GPS          PASS   1460 bytes, 24 NMEA lines in 3000 ms (state NOFIX)
I bringup: ---------------- 9 PASS  0 FAIL  0 ABSENT ----------------
```

The same result goes to `idl0_debug.log` on the SD card as one `selftest ...`
line.

LEDs: green (D2) blinks at 1 Hz when the board is powered with no link. It is
solid while the phone is connected over BLE or the WiFi AP is up. Red (D1) is
solid while a session is recording. If the TLA2528 is not up, both stay dark.
The XIAO's onboard LED keeps its old behaviour in every case: slow blink when
idle, fast blink while recording.

The BLE status line shows `IMU: OK` with all three IMUs running. It shows
`IMU: ERROR` if a pod is unplugged, because an enabled pod that does not answer
counts as down. To run without a pod, disable it in the config
(`imu.imu2.enabled: false`).

To test the button, hold SW1 during the 3 s window. You should see
`press seen ... yes`. Holding it from reset gives FAIL (`LOW: held or shorted`).

## When a row is not PASS

| Row | Meaning / what to do |
|---|---|
| **PWR_EN FAIL**, or everything else FAIL at once | The switched rail is off. Check GPIO2 → U2 (TPS22919) ON pin and U2's output. Nothing but the MCU works without it. |
| **TLA2528 FAIL, no ACK at 0x10** | This is the expected failure on the boards as built. ADDR is shorted to GND through copper net tie **NT1**, a configuration TI does not document, so the address is unknown (decisions.md H2). The firmware drives the TLA2528 at one address only, but logs every address that answers just before the self-test (`I2C devices answering: …`): the TLA2528 at 0x11–0x17 means ADDR mis-decoded; answering nowhere means the chip is wedged. Since 0.3.0-beta.13 every boot holds the switched rail off for 100 ms first, so a software reset (OTA, config apply) is a real power-on for the TLA2528 and pods instead of a brownout. The rest of boot carries on without the board LEDs or analog. Two ways out:<br>1. **Try a candidate:** edit `IDL1_I2C_ADDR_TLA` in `main/pins.h` (one line, try 0x10–0x17), rebuild, reflash and read the row again.<br>2. **Cut NT1** (knife through the copper between its pads). This gives the documented 0x10. It is not cleanly reversible. |
| TLA2528 FAIL, ACK but reserved-bit check fails | Something answered at that address but it is not behaving like a TLA2528 (SYSTEM_STATUS bit 7 should be 1, bits 4 and 1 should be 0). Check the raw byte. |
| **POD F / POD R ABSENT** | No ACK at that address. Check the M8 cable and the pod's JP1: front = JP1 **open** = 0x6A, rear = JP1 **closed** = 0x6B (decisions.md P7·H3). If both pods are jumpered the same way, one of them shows at the other's address. An unplugged pod is a normal configuration. |
| POD / IMU0 FAIL with an unexpected WHO_AM_I | A chip answered, but it is not an LSM6DSV320X (0x73). Note the raw value. |
| POD / IMU0 FAIL, 0x73 but configuration FAILED | The chip is right but a register write failed. The log names the step (`IMUn init: <step> failed`). |
| **IMU0 SPI FAIL**, WHO_AM_I 0x00 or 0xFF | Nothing is driving MISO. Check the rail, CS on GPIO21, and SPI 18/19/20. |
| **SD FAIL** | The card is missing, unformatted (FAT) or unreadable. CS is GPIO7 on IDL1. |
| **BATTERY ABSENT / FAIL** | ABSENT means under 0.5 V: no cell, or the divider is open. FAIL means outside 3.0–4.35 V: check the reading against a meter at BAT+ (Vbat = 1.5 × pin). |
| **GPS FAIL** | 0 bytes means no UART traffic. Check the rail and the module TX → GPIO17 link. Bytes with 0 NMEA lines means a baud or wiring problem. The module has 9600 baud factory defaults. |

## Phase 2 bench checks

IMU defaults: 480 Hz shared by all three IMUs, ±16 g, ±2000 dps. Values the
idl0 app sends that the LSM6DSV320X cannot do (for example 833 Hz, 32 g or
125 dps) are rejected field by field, with a `config` warning in the log.
The previous value is kept.

1. **Overruns at 480 Hz.** Idle, with no session running, the serial log
   prints this line every 5 s:
   `drained over last 5000 ms — IMU0: ~2400 pairs ovr=0 | IMU1: ~2400 pairs ovr=0 | IMU2: ...`.
   Every `ovr` must stay 0. After a session, `idl0_debug.log` has one line
   per IMU: `IMUn spi|i2c odr_hz=480 drains=... pairs=... ovr=0 ...`. Check
   that `ovr=0` and `unk=0` for both pods.
2. **Try 960 Hz.** Set `"imu": { "sample_rate_hz": 960 }` in
   `idl0_config.json` (app config push or edit the card), then reboot. Repeat
   check 1 and expect ~4800 pairs per 5 s. Any `ovr` above 0 on a pod means
   the shared I2C bus cannot keep up, so stay at 480 (decisions.md P2).
3. **IMU0 and SD share SPI (A10).** Record a session of at least 10 minutes
   at the rate you are testing. In the session-end diag lines,
   `writer_drops=0`, IMU0 `ovr=0`, and IMU0 `pairs` ≈ rate × seconds. The
   session must not stop on its own. An SD stall of more than 3 s now marks
   the card ERROR and stops the session (T38) rather than hanging.
4. **Pod pulled mid-session (A5 / T41).** Unplug a pod while recording. The
   file keeps getting zero-filled IMU_SAMPLE records for that IMU, one per
   20 ms poll, instead of simply going silent. At the next session start the
   firmware tries that pod once more.
5. **Orientation sanity.** With the board flat and still, accel Z reads about
   +1 g in the app plot (registry scale 0.488 mg/LSB at ±16 g) and gyro reads
   near 0.

## OTA

The firmware only accepts OTA images whose `project_name` is `idl1_firmware`.
An idl0-firmware image offered by the idl0 app gets a 400 error with
"image is for project idl0_firmware, this board runs idl1_firmware", and
nothing is written to flash (decisions.md F5).
