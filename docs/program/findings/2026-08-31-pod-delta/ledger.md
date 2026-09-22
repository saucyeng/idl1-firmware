# Overnight ledger — run: 2026-08-31-pod-delta — started 2026-08-31T21:15:00-05:00

Plan: idl1-program/02-pod-sensor-delta.md. Contract: idl1-program/CONTRACT.md.

## Input check (orchestrator, before any dispatch)
- lsm6dso32.{md,tables.json}: present (453 KB / 520 KB). specs.json is a 97-byte placeholder — ignored.
- lsm6dsv320x.{md,tables.json}: present (673 KB / 700 KB). specs.json is a 99-byte placeholder — ignored.
- ATS601.{md,tables.json}: present. No specs.json exists (plan did not list one).
- 500 psi pressure sensor: source PDF present at references/source/"500psi pressure sensor.pdf"; NO extraction exists under references/extracted/ (glob for psi|press returned nothing). Decision below.
- idl0-firmware IMU path: imu_driver.c (561 l), imu_driver_i2c.c (482 l), imu_driver.h (105 l), imu_task.c (371 l), pins.h (66 l), components/lsm6dso32x_STdC/ (reg.h 4325 l, reg.c 12416 l) — all present.
- Spec: idl0-app/docs/IDL0_SPEC.md present.

## Decisions (contained)
- D1 (contained): 500 psi pressure sensor had no docling extraction (catalog.json lists it with null artifacts). Ran `tools/docling-scaffold/src/parse.py` on the source PDF to produce `references/extracted/500psi pressure sensor.{md,tables.json}`. Derived artifacts only; no design source touched. If the parse fails, Pass 3 reads the PDF directly and cites PDF page numbers.
- D2 (local): page-cite convention for Pass 1/3 — extracted .md has no page markers; agents cite the `pages` field of the tables.json table or the datasheet TOC page of the section. Recorded in idl1-program/briefs/pod-regs-common.md.
- Note: idl1-program/briefs/spec-*.md were written 21:12 by the concurrent 01-idl0-audit run. This run prefixes its briefs `pod-` to avoid collisions.
- D3 (user, 21:20): "don't worry about the exact pressure sensor yet — just have it prepped to do basic analog input." Pass 3 pressure scope becomes: generic analog input on the pod (pin, divider/attenuation, ADC config, sample rate), not the 500 psi part's transfer function. The docling extraction from D1 finishes anyway; it is not an input.

## Dispatches
- 21:21 Pass 1 dispatched: 6 x haiku (regs-identity-boot, regs-ctrl, regs-fifo, regs-interrupts, regs-spi, regs-selftest). Brief: idl1-program/briefs/pod-regs-common.md
- D1 result: docling parse of the 500 psi PDF exited 0 but produced an EMPTY .md (0 bytes) and a 208-byte tables.json — the PDF is image-only and needs `parse.py --ocr`. Not re-run: moot after D3. Morning note: re-extract with --ocr when the transducer is chosen.
- Orchestrator check (21:30): pod BOM (docs/datasheet/breakout/data/board-facts.md) = IMU1 LSM6DSV320XTR, U1 ATS601LSG, J1 6-wire breakout connector, J2 3-wire analog connector (JST PH), R1 10K, U2 TPD6E05U06 ESD. Six wires cannot carry 4-wire SPI + wheel + analog + power, so the pod IMU bus is being verified from the netlist before Pass 2/3 briefs are finalised.
- WIRING VERIFIED (21:35, from breakout-netlist.xml + idl1 board-facts.md): pod J1 6-wire = GND, +3V3, SDA, SCL, WHEEL, ANA. Pod IMU1 (DSV320X) is on I2C — CS tied +3V3, INT1/INT2 unconnected, SDO/TA0 via JP1 + R1 10K. ATS601 OUT -> WHEEL -> main-board XIAO GPIO0 (front) / GPIO1 (rear), 4k7 pull-up + 10nF + 330R series on the main board. Pod ANA (J2 3-wire, 3V3-powered) -> main-board TLA2528 AIN5 (front) / AIN4 (rear) via 33R + 100nF; TLA2528 at I2C 0x17 shares the bus with both pod IMUs. Plan's premise "SPI specifics" is the wrong bus for the pod: regs-spi kept (reference), regs-i2c added.
- 21:36 Pass 1 extra dispatch: 1 x haiku regs-i2c (topic 7). Briefs written: idl1-program/briefs/pod-new-sensors.md (Pass 3), idl1-program/briefs/pod-driver-impact.md (Pass 2).
- FLEET FACT (21:40, idl1 board-facts L53/101-108/220-223): IDL1 main-board onboard IMU is ALSO an LSM6DSV320X, on SPI (CS GPIO21, bus shared with microSD). DSO32 disappears entirely on IDL1. One chip type, two buses. Pass 2 brief updated (context + Q5).
Task 1a: complete — idl1-program/findings/2026-08-31-pod-delta/regs-identity-boot.md
Task 1b: complete — idl1-program/findings/2026-08-31-pod-delta/regs-ctrl.md
Task 1c: complete — idl1-program/findings/2026-08-31-pod-delta/regs-fifo.md
Task 1d: complete — idl1-program/findings/2026-08-31-pod-delta/regs-interrupts.md
Task 1e: complete — idl1-program/findings/2026-08-31-pod-delta/regs-spi.md
Task 1f: complete — idl1-program/findings/2026-08-31-pod-delta/regs-selftest.md
- 21:41 Pass 3 dispatched: 1 x haiku new-sensors. Brief: idl1-program/briefs/pod-new-sensors.md (+ digital_task.c pointer in the dispatch).
Task 1g: complete — idl1-program/findings/2026-08-31-pod-delta/regs-i2c.md
- 21:52 Pass 2 dispatched: 1 x sonnet driver-impact. Brief: idl1-program/briefs/pod-driver-impact.md. Told it IF_INC = bit 2 (verified in extraction; regs-spi.md bit numbers wrong, regs-ctrl.md right).
- Review note: regs-spi.md rows "IF_INC ... bit [3] / bit [4]" and "SIM ... bit [1]" are mis-numbered (orchestrator verified CTRL3 layout in lsm6dsv320x.md line 2028). Values/addresses in that file otherwise consistent with regs-ctrl.md. Annotated at the bottom of regs-spi.md.
- Review note (21:58): regs-interrupts.md STATUS_REG "XLDA/GDA/TDA shifted" is an off-by-one — verified line 2320: positions unchanged (bits 0/1/2), XLHGDA = bit 3. Annotated. Same column-miscount pattern as regs-spi.md. Mitigation: dispatching a haiku bit-map audit (regs-bitmap.md) that transcribes each register's bit row verbatim with a line cite.
Task 3: complete — idl1-program/findings/2026-08-31-pod-delta/new-sensors.md
- 22:03 Pass 1h dispatched: 1 x haiku regs-bitmap (verbatim bit rows + field encodings, line-cited). Sent STATUS_REG/IF_CFG correction to the running Pass 2 agent.
- Review note (22:08): new-sensors.md — ATS601, wiring and RC sections sound; TLA2528 "register sequence" table uses addresses that do not match the part's register map (CHANNEL_SEL is 0x11 not 0x09; OSR_CFG 0x03 not 0x06) and omits the opcode-based I2C protocol. Dispatched 1 x haiku tla2528-regmap (verbatim register map / opcodes / data format). Checklist will cite that file, not the sequence table.
- FACT (22:12, esp32-c6-datasheet.md L2836-2845 §4.2.1.3): ESP32-C6 I2C controller supports standard (100 kbit/s) and fast (400 kbit/s) only. Pod bus ceiling = 400 kHz regardless of DSV320X 1 MHz / TLA2528 HS support. Orchestrator estimate at 400 kHz: 12-byte direct read of one IMU ≈ 15 bytes x 22.5 us ≈ 0.35 ms; two pods at 960 Hz ≈ 65-70 % bus utilisation before polling overhead -> 480 Hz comfortable, 960 Hz marginal. To be reconciled with Pass 2's arithmetic.
- 22:12 waiting on: driver-impact (sonnet), regs-bitmap (haiku), tla2528-regmap (haiku). questions.md drafted with Q1-Q7.
- FACT (22:15, idl1 board-facts L100/109/132/159/170/270-292): XIAO GPIO2 = PWR_EN drives TPS22919 U2; its switched +3V3 feeds the onboard IMU VDD, both pod connectors (J4/J5 pin 2) and J6. Power-sequencing step exists: PWR_EN high -> wait (TPS22919 rise + DSV320X boot + gyro Ton 40 ms + ATS601 <=1 ms + TLA2528 tPU) -> probe. No IMU INT1/INT2 nets exist anywhere on IDL1 (grep empty) -> all three IMUs are polled, onboard included.
Task 3b: complete — idl1-program/findings/2026-08-31-pod-delta/tla2528-regmap.md
Task 1h: complete — idl1-program/findings/2026-08-31-pod-delta/regs-bitmap.md
- Review note (21:30 real time — earlier ledger timestamps from 21:36 onward were the orchestrator's estimates and run ~50 min fast; file mtimes are authoritative): regs-bitmap.md DSV320X columns verified on 5 registers; DSO32 columns unreliable (image placeholders; STATUS_REG cell mis-cited to an OUT_TEMP table). Annotated. DSO32 authority = vendored lsm6dso32x_reg.h.
Task 2: complete — idl1-program/findings/2026-08-31-pod-delta/driver-impact.md

## Synthesis (orchestrator, real time 21:34)
- Reviewed: regs-bitmap.md (DSV320X columns verified on 5 registers), tla2528-regmap.md (register map, opcodes, timing, address table), new-sensors.md (ATS601 + paths sound; TLA2528 sequence table superseded), driver-impact.md (read in full; its 1 MHz bus recommendation overridden by the C6 datasheet's 400 kbit/s limit — logged as checklist A9, a judgment item).
- Contained decision: wheel speed uses the GPIO-ISR per-pulse pattern (spec §3.5, digital_task.c) rather than PCNT — PCNT cannot produce the per-pulse timestamped records the spec mandates.
Task 4: complete — idl1-program/findings/2026-08-31-pod-delta/questions.md (Q1–Q7)
Task 5: complete — idl1-program/findings/2026-08-31-pod-delta/PORT-CHECKLIST.md (34 work items + 30 register mappings; 8 PROVISIONAL)
Task 6: complete — idl1-program/findings/2026-08-31-pod-delta/SUMMARY.md
- Queue empty. Run finished 21:34. Scratch briefs: idl1-program/briefs/pod-regs-common.md, pod-driver-impact.md, pod-new-sensors.md.
