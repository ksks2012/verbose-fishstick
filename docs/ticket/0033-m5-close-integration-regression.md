# 0033 — M5 close: integration / regression matrix + manual docking smoke

**Source:** ADR-0011 M5 spec (Q8, Testing Decisions); M5 close, mirroring 0024's role for M4.
**Status:** ready-for-agent
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

- [ ] CTest green in both configurations, with the new app-layer editor suites included in the
      reported count
- [ ] Structured runtime-log assertions cover, over the CLI entry modes: entity counts, the entity
      lifecycle (create / destroy), selection, asset assignment, and transform commits including the
      dual-unit transform line
- [ ] The asset picker's logged counts (offered / hidden) are asserted where a picker interaction is
      driven
- [ ] The M4 CLI exit-code matrix is unchanged: all previously valid invocations still exit 0, all
      previously invalid still exit 1
- [ ] `M0_OK`, `M3_INFO`, `M4_INFO` and `M2_OK` log content and ordering unchanged where the
      existing paths still run
- [ ] `--list-assets` output unchanged
- [ ] The single resolution seam still holds: no path reaches the renderer by bypassing
      `collectDrawItems`, and no new `Model`/`Mesh`/`Texture`/GPU-resource ownership was introduced
- [ ] The app layer still links neither ImGui nor GLFW

## Acceptance — manual / platform-specific (recorded here, not automated)

- [ ] `xprop` `WM_NAME` titles re-verified for every CLI entry mode, unchanged from M4
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

- [ ] `CONTEXT.md` updated with the M5 milestone state and the preserved contracts; `CHANGELOG.md`
      given the M5 summary (both in the same change, per the document hierarchy)
- [ ] ADR-0011's scope exclusions re-confirmed as still out of scope, and any deviation found during
      implementation recorded as an ADR amendment rather than a silent edit
- [ ] 0028–0034 archived under the accepted ADRs; ADR-0011 left unmodified unless superseded
- [ ] 0026 recorded as an M5-adjacent follow-up (still open, now waiting only on 0028), and 0027
      still an M6-preceding gate
- [ ] Milestone close evidence summarised in this ticket, and an execution summary written under
      `docs/summary/`

**Points to ADR:** 0011 (Q8 verification tiers and logging, Testing Decisions, Out of Scope) ·
0010 (Q8 the M4 close shape this ticket mirrors) · 0008 (document hierarchy: CONTEXT + CHANGELOG
updated in the same change) · DOCUMENTATION-LIFECYCLE (archive on close, extract durable knowledge)
