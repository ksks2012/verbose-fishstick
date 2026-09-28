# 0026 — M5 camera interaction semantics & cursor baseline

**Source:** code review of 0021–0024 (items ⑥⑦), split out of 0025 in the Sep 2026 re-review.
**Status:** active in M5 — **blocked by the first M5 slice's input-ownership gate**
**Blocked by:** ADR-0011 Q9/Q10 — the first slice lands viewport containment + ImGui item
arbitration + wheel gating. This ticket defines camera *semantics and baselines underneath that
gate*; it does not re-decide input ownership.
**Points to ADR:** 0011 (Q9/Q10 gate, Q6 `ViewportState` owns the `Camera`) · 0010 (Q7 camera
purity) · 0005 (ImGui pass handoff, wheel ownership)

**Scope narrowed 2026-09-28 by ADR-0011.** This ticket used to be "M5 input & interaction
configuration", which would have had it and the M5 spec each defining who owns input. ADR-0011
Q9/Q10 fixed that centrally: the gate is `viewportHovered && !IsAnyItemHovered() &&
!IsAnyItemActive() && !globalModalCapture`, and the wheel obeys the same gate. 0026 now owns only
orbit/pan/zoom input semantics, delta computation, sensitivity configuration, and cursor
baselines. Do not add a second input-ownership predicate here.

**Two corrections from the 2026-09-28 review of the current implementation:**
- `InputState::prevX/prevY` are updated **every frame** and `firstMove` suppresses only the first
  sample. There is **no per-drag stale delta**; the concern is baseline *re-establishment* at
  ownership transitions, not a delta that lags within a drag.
- Sensitivity constants (`kOrbitSens` / `kPanSens` / `kZoomSens`) are already in the app/input
  layer, not in `renderer::Camera`, so the purity boundary below currently holds.

## ① Camera interaction semantics under the gate

With ownership fixed by ADR-0011, define the remaining camera behaviour:

- **Gesture → operation mapping** and its relationship to ADR-0010 Q7's locked numeric contract
  (orbit yaw/pitch, zoom distance clamp `[0.5, 20]`, pan in world units). The Camera contract is
  already signed and unit-tested; this ticket decides how pixels become those inputs, not the
  signs themselves.
- **Wheel routing**: consumed only when the gate allows it, applied as `camera.zoom(...)`; the
  delta is discarded (not queued) when the gate denies it, so a scroll over a panel never becomes
  a later zoom.

## ② Cursor baseline re-establishment (was 0025⑥)

`InputState::firstMove` starts `true` and suppresses the first cursor sample after interaction
begins. It is about **cursor ownership**, not scene lifecycle — the premise "scene reload ⇒ reset
input state" was unproven and is not adopted.

**Define (at M5):** which events require the cursor delta baseline to be re-established, and
whether the state belongs in `InputState` at all. Known triggers, from the ADR-0011 Q10
consequences and current code:

- **End of an ImGui widget drag.** While a widget drag runs, the gate denies the camera; when it
  ends the cursor is wherever the drag left it, so the next frame's delta would otherwise be a
  large spurious jump. This is a **baseline** problem, not an ownership problem.
- **Window focus regain** (e.g. alt-tab while a button is held): the current callbacks do not
  re-baseline, so a release after refocus can be read as a large delta — a narrow-frequency
  stuck-button case, not a current defect in normal use.
- **Start/end of a camera drag** and any future cursor-capture or warp event.

**Do not** couple any of this to `AssetRegistry::reload()` until that relationship is designed.

## ③ Camera interaction sensitivity configuration (was 0025⑦)

Runtime tuning (ImGui sliders) is a legitimate M5 want, but **ownership must not be decided now**
and must **not** move into the renderer.

**Hard boundary (ADR-0010 Q7):** `renderer::Camera` stays pure math — it knows
`orbit(dx,dy) / zoom(delta) / pan(dx,dy)` and nothing about pixels. Pixel→radians / pixel→world
conversion is an **app/input-layer** concern, and the 0023 sign/unit tests depend on the Camera
contract staying free of it.

**M5 to decide:** the owning type name and scope (`InteractionSettings`? `EditorPreferences`?
`CameraInputConfig`?) — do **not** commit to `CameraPrefs` or any other name pre-emptively; add the
knobs in the layer that already maps gestures, and re-review before extracting a type. Note the
`Camera` is now owned by `ViewportState` (ADR-0011 Q6), so a per-view sensitivity would need to say
whether it is per-view or session-wide.

## Acceptance

- [ ] ① Orbit/pan/zoom gesture mapping and wheel routing defined **under** the ADR-0011 gate, with
      no second input-ownership predicate introduced
- [ ] ② Cursor-baseline re-establishment defined for the events the editor actually introduces
      (at minimum: widget-drag end, focus regain, camera drag start/end)
- [ ] ③ Sensitivity configuration owned by the app/editor input layer, not `renderer::Camera`, with
      its per-view vs session scope stated
- [ ] 0023 camera tests (`tests/renderer/camera_test.cpp`) stay green and unchanged in spirit
- [ ] Wheel deltas denied by the gate are discarded, not deferred

