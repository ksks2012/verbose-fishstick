# 0024 — M4 integration, regression & close

**What to build:**

The M4 close ticket (per ADR-0010 Q8): an **automated regression matrix** (dev + release) plus a
**single lightweight manual smoke check** recorded as evidence in this ticket. No new feature, no
separate human-validation ticket, and the M3 readback/similarity exclusions stay locked.

## Scope

**A — automated regression (dev + release):**

CLI exit-code matrix:
| Path | Expected |
| --- | --- |
| default | exit 0 |
| `--asset=smile` / `--asset=crown` / `--asset=sunglasses` | exit 0 |
| `--demo` | exit 0 |
| unknown `--asset` | exit 1 |
| `--asset` + `--demo` | exit 1 |
| invalid flag | exit 1 |

Structured runtime-log assertions (the process-level window into the pipeline — no `--dump-scene`,
no Scene internals exposed):
- default ⇒ `M4_INFO rendering scene 'm4_vertical_scene' entities=3`
- `--asset=<id>` ⇒ `M4_INFO rendering scene '<id>' entities=1` (plus the M3 identity line)
- `--demo` ⇒ `M4_INFO rendering scene 'demo_checker' entities=1` + `M2_OK (0009 E2E)`
- **Unresolved-asset → skip is the 0021 headless resolution contract** (`tests/app/`); 0024 does
  not add a production CLI entry to exercise it.
- Existing CTest suites green (dev + release) with the new M4 suites (`tests/scene/`, `tests/app/`,
  camera in `tests/renderer/`) included in the count.

**B — manual / platform-specific evidence (Linux/X11), recorded in this ticket:**
- `xprop` `WM_NAME` title verification: default `m4_vertical_scene | noto3d (M4)`;
  `--asset=<id>` → `<id> [placeholder] | noto3d (M4)`; `--demo` → `demo_checker [placeholder] |
  noto3d (M4)`.
- Manual smoke check (once, ~10 min): `smile`(left) · `crown`(center) · `sunglasses`(right) layout
  is correct; LMB drag orbits without pole-flip; wheel zooms; MMB drag pans; ESC exits.
- Acceptance = manual confirmation of the expected fixture layout and interaction behavior; it is
  **not** a rendering-correctness test.

**C — explicitly excluded (locked):**
- Screenshot comparison, GL readback, image similarity (`glReadPixels` + hash + threshold is out of
  scope even *with* three entities), separate human-validation ticket, new feature work.

## Acceptance

- [x] Full CLI matrix green on dev **and** release (incl. new invalid invocations)
- [x] Structured runtime-log assertions verify Scene construction for all three entry modes
      (default=3 / `--asset`=1 / `--demo`=1 demo_checker); no `--dump-scene`
- [x] Unresolved-asset → skip proven by 0021's headless `tests/app/` suite (no new 0024 CLI entry)
- [x] xprop `WM_NAME` titles verified as manual evidence (default, `--asset`, `--demo` modes)
- [x] Manual smoke check recorded (layout + orbit/zoom/pan + ESC); acceptance = interaction
      semantics only, not rendering correctness
- [x] No readback/similarity/screenshot work introduced
- [x] M4 closed: CHANGELOG + CONTEXT updated, tickets 0020–0024 archived

**Blocked by:** 0023 (complete)
**Status:** completed

> Evidence (2026-09-27): **A — automated (dev AND release):** CLI exit-code matrix all green —
> default / `--asset=smile|crown|sunglasses` / `--demo` → 0; unknown `--asset=nope`,
> `--asset=smile --demo`, invalid flag `--bogus` (hardened in 0022), `--frames=abc` → 1. Structured
> runtime-log assertions (`M4_INFO rendering scene '<name>' entities=<N>`): default
> `m4_vertical_scene`=3, `--asset=smile`=1 (+ `M3_INFO selected asset ...` identity line), `--demo`
> `demo_checker`=1 (+ `M2_OK (0009 E2E)`). Unresolved-asset → skip proven headlessly by 0021's
> `tests/app/` suite; no `--dump-scene`, no new production CLI entry. CTest 8/8 on dev + release
> (includes `tests/scene/`, `tests/app/`, camera in `tests/renderer/`).
> **B — manual evidence (X11, DISPLAY=:0):** xprop `WM_NAME` verified for all three modes —
> `m4_vertical_scene | noto3d (M4)`, `smile [placeholder] | noto3d (M4)`,
> `demo_checker [placeholder] | noto3d (M4)`. Manual smoke (user-confirmed): three-entity layout
> correct (smile left / crown center / sunglasses right), LMB drag orbits without pole-flip, wheel
> zooms, MMB drag pans, ESC exits — interaction semantics only.
> **C — exclusions honored:** no readback/similarity/screenshot work introduced. **M4 closed**:
> CHANGELOG + CONTEXT updated in the same change; tickets 0020–0024 archived.
> Note: `--frames=-1` is rejected by the validated `--frames=` parser by design (non-negative
> integer contract, 0011-⑨); unlimited = omit the flag — irrelevant to the matrix.
**Points to ADR:** 0010 (Q6/Q8) · 0009 (M3 exclusions continue)
**Rolls under spec:** ADR-0010 (M4 spec embedded; execution tickets only in docs/ticket/)