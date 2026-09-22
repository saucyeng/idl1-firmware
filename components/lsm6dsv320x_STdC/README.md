# lsm6dsv320x_STdC (vendored)

Source: https://github.com/STMicroelectronics/lsm6dsv320x-pid
Tag pulled: v5.4.0 (vendored 2026-09-22)
License: BSD-3-Clause (see ./LICENSE)

SHA-256 of the files as pulled:

| File | SHA-256 |
|---|---|
| `lsm6dsv320x_reg.c` | `e070b5d49790bf93931e84f8b72b16ad4edf333366eb4e0f6161cfb6f144ae1a` |
| `lsm6dsv320x_reg.h` | `2d097728229d6c83ac9232f828272428dbdd9c59a9b179747050a8cf109ea90f` |
| `LICENSE` | `b54f36a43cf9b8f6292c8f0f3b4c267ebd59859587e0152c94305bc81dda68bc` |

This is the platform-independent C driver provided by STMicroelectronics.
The ESP-IDF bus adapters (the `platform_read` / `platform_write` /
`platform_mdelay` function pointers that `stmdev_ctx_t` requires) live in
`../../main/imu_driver.c` (SPI, onboard IMU0) and `../../main/imu_driver_i2c.c`
(I2C, pods). The chip-level sequence that uses this driver is in
`../../main/imu_chip_dsv320x.c`. Nothing in this folder is edited, so it stays
a pristine copy of upstream and can be re-synced cleanly.

To update, pull the new tag's `lsm6dsv320x_reg.c`/`.h` and `LICENSE`,
check the diff, and update the tag and hashes above.
