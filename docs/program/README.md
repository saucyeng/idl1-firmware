# Program docs (IDL1 firmware)

Copied from the local `idl1-program/` planning folder on 2026-09-22 so the
bring-up machine has them. **From here on these copies are canonical**; edit
them here, not in `idl1-program/`.

Paths inside these files written as `idl1-program/X` mean `docs/program/X`
(`plans/` files sit at this directory's top level).

| File | Role |
|---|---|
| `decisions.md` | Owner rulings. Authority over everything else. F1–F5 = bring-up week. |
| `2026-09-22-idl1-bringup-phases.md` | The three phases and review corrections R1–R10. Start here. |
| `pod-firmware-plan.md` | 24 register-level tasks (PT1–PT24), re-sliced by the phases file. |
| `findings/2026-09-02-pins-idl1/pins_idl1-spec.md` | Pin map source (namespace rules, collision audit). |
| `findings/2026-08-31-hw-delta/` | Netlist-derived IDL0→IDL1 pin delta. |
| `findings/2026-08-31-pod-delta/` | LSM6DSV320X / TLA2528 register extractions. |
| `findings/2026-08-31-idl0-audit/triage.md` | Audit rows; T38/T41 still open. |
