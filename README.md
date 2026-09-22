# idl1 — firmware

ESP32-C6 firmware for the IDL1 mountain-bike data logger — the board revision
with M8 sensor pods on a shared I2C bus, a TLA2528 analog front end, and a
switched 3V3 rail. During a logging session it does raw sensor capture only —
sensor bytes straight to the SD card, no processing. Everything else (BLE/WiFi
control, OTA update, calibration, file transfer) runs outside the logging path.

Forked from `idl0-firmware` (now the frozen IDL0 firmware). The wire format and
BLE contract are still the IDL0 ones — the idl0 app talks to this firmware
unchanged. Program docs (rulings, bring-up plan) live in `docs/program/`; the
bench checklist is `docs/bringup.md`.

## Build & flash (ESP-IDF v6.0.1, target esp32c6)

```
idf.py set-target esp32c6
idf.py -p /dev/ttyACM0 flash monitor    # first flash over USB
```

The **first** flash must be over USB — it lays down the bootloader, partition
table, and first slot. Every update after that ships over-the-air: tag a release
(`git tag v1.5.0 && git push --tags`) and the app installs it from GitHub Releases.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE). (GPLv3, not v2, for compatibility with
ESP-IDF's Apache-2.0.) Vendored components under `components/` and
`managed_components/` keep their own licenses. Contributions require the CLA.
