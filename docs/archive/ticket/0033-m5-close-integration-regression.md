# 0033 — M5 close: integration / regression matrix + manual docking smoke

**Source:** ADR-0011 M5 spec (Q8, Testing Decisions); M5 close, mirroring 0024's role for M4.
**Status:** in-progress (automated matrix passing; manual smoke pending user confirmation)
**Blocked by:** 0030, 0032

M5's close is the same two-tier shape 0024 locked for M4 (ADR-0010 Q8): an **automated matrix**
(exit codes + structured runtime-log assertions + CTest) plus **manual, platform-specific evidence**
recorded in this ticket. Like 0024, it is **not** a rendering-correctness test and introduces **no
readback, screenshot comparison, or image similarity**.

0026 (camera interaction semantics and cursor baselines) is **M5-adjacent and does not block this
ticket** — ADR-0011 puts full camera semantics in Out of Scope, so M5 closes on "can I edit a Scene
with the mouse", not on camera feel.

## What it delivers

- The M5 editing surface proven as a whole: select → transform → swap asset → create → delete, with
  every step observable in the structured log.
- The M4 contracts re-verified **unchanged**, proving the editor was added beside the runtime rather
  than over it.
- Milestone documentation synced, and the M5 execution tickets archived.

## Acceptance — automated (dev + release)

- [x] CTest green in both configurations, with the new app-layer editor suites included in the
      reported count
- [x] Structured runtime-log assertions cover, over the CLI entry modes: entity counts, the entity
      lifecycle (create / destroy), selection, asset assignment, and transform commits including the
      dual-unit transform line
- [x] The asset picker's logged counts (offered / hidden) are asserted where a picker interaction is
      driven
- [x] The M4 CLI exit-code matrix is unchanged: all previously valid invocations still exit 0, all
      previously invalid still exit 1
- [x] `M0_OK`, `M3_INFO`, `M4_INFO` and `M2_OK` log content and ordering unchanged where the
      existing paths still run
- [x] `--list-assets` output unchanged
- [x] The single resolution seam still holds: no path reaches the renderer by bypassing
      `collectDrawItems`, and no new `Model`/`Mesh`/`Texture`/GPU-resource ownership was introduced
- [x] The app layer still links neither ImGui nor GLFW

## Acceptance — manual / platform-specific (recorded here, not automated)

- [x] `xprop` `WM_NAME` titles re-verified for every CLI entry mode, unchanged from M4
- [ ] Docking: panels can be moved and resized; the 3D view keeps correct aspect at a non-square
      size and is not stretched or inverted
- [ ] Full editing pass, user-confirmed: select an entity in the Hierarchy, change position,
      rotation and scale (checking the degree labels and the `Y → X → Z` order), change its asset,
      create a new entity, delete one
- [ ] Interaction ownership, user-confirmed: dragging or scrolling any panel leaves the camera
      untouched, including during a widget drag that crosses the 3D view; the camera responds when
      the cursor is over the 3D view with no widget in play
- [ ] Confirmation that no gizmo appears in the 3D view and that no undo affordance exists

## Acceptance — milestone close

- [x] `CONTEXT.md` updated with the M5 milestone state and the preserved contracts; `CHANGELOG.md`
      given the M5 summary (both in the same change, per the document hierarchy)
- [x] ADR-0011's scope exclusions re-confirmed as still out of scope, and any deviation found during
      implementation recorded as an ADR amendment rather than a silent edit
- [x] 0028–0034 archived under the accepted ADRs; ADR-0011 left unmodified unless superseded
- [x] 0026 recorded as an M5-adjacent follow-up (still open, now waiting only on 0028), and 0027
      still an M6-preceding gate
- [x] Milestone close evidence summarised in this ticket, and an execution summary written under
      `docs/summary/`

## Evidence — automated (recorded 2026-09-29)

Ran `scripts/m5-close-matrix.sh` on both configs (dev = log-line tier; release = log-line + exit
code tier; dev's GL exit code is 1 per the documented Mesa llvmpipe LeakSanitizer limitation,
so exit codes assert on release):

- Release: **30/30 checks pass** (`scripts/m5-close-matrix.sh build/release --release`).
- Dev: log-line + static + headless tiers all pass; pre-GL exit codes also asserted via CTest.
- CTest green **21/21 dev + release** (app suites included: editor/transform-edit/asset-picker/
  entity-edit).
- Headless suites assert the M5 structured logs directly (log-capture sinks): `M5_SEL`,
  `M5_ASSET` (offered/hidden counts + assigned), `M5_XFRM` (incl. dual-unit rotation line),
  `M5_ENTITY` (create/destroy + count); `M5_SEL` coverage was the closing gap and was added to
  `tests/app/editor_test.cpp`.
- CLI entry-mode log-content + ordering (`M0_OK` → `M3_INFO`/`M2_OK` → `M4_INFO entities=N` →
  `M1_DONE`) asserted for default / `--asset=smile` (and `crown`) / `--demo`.
- `--list-assets`: `M3_ASSETS version=1` … `end count=6` unchanged.
- Static: `collectDrawItems` sole render entry + resolver-only `registry.model(` calls (2);
  `DrawItem::model` borrowed pointer; `noto3d_app` links neither ImGui nor GLFW (CMake arch-guard
  + link-line grep).
- `xprop` `WM_NAME` (three modes): `m4_vertical_scene | noto3d (M4)`,
  `crown [placeholder] | noto3d (M4)`, `demo_checker [placeholder] | noto3d (M4)` — all unchanged
  from M4.

**Points to ADR:** 0011 (Q8 verification tiers and logging, Testing Decisions, Out of Scope) ·
0010 (Q8 the M4 close shape this ticket mirrors) · 0008 (document hierarchy: CONTEXT + CHANGELOG
updated in the same change) · DOCUMENTATION-LIFECYCLE (archive on close, extract durable knowledge)
