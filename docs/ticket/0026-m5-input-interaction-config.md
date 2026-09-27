# 0026 — M5 input & interaction configuration follow-ups

**Source:** code review of 0021–0024 (items ⑥⑦), split out of 0025 in the Sep 2026 re-review.
**Status:** deferred (open when M5 scope opens)
**Blocked by:** M5 opening (ImGui editor milestone, ADR-0005 handoff at `Renderer::End3DPass()`)

Not M4 work. Recorded here so the M4 cleanup ticket stays free of M5 feature design, and so the
questions are not lost.

---

## ① InputState cursor-baseline reset semantics (was 0025⑥)

`InputState::firstMove` starts `true` and is used to suppress the first cursor sample after
interaction begins. There is **no reload path at M4**, so this is a speculative concern, not a
current defect. Also note the premise "scene reload ⇒ reset input state" is unproven: `firstMove`
is about **cursor ownership**, not scene lifecycle.

**Define (at M5):** which events invalidate the cursor delta baseline — e.g. cursor capture,
window focus regain, ImGui mouse-capture transitions, start/end of a camera drag. Then decide
whether a reset is needed per event, and whether the state belongs in `InputState` at all.

**Do not** couple the answer to `AssetRegistry::reload()` until that relationship is designed.

---

## ② Camera interaction sensitivity configuration (was 0025⑦)

`kOrbitSens` / `kPanSens` / `kZoomSens` are `constexpr float` in `main.cpp`. Runtime tuning is a
legitimate M5 want (ImGui sliders), but **ownership must not be decided now** and must **not** move
into the renderer.

**Hard boundary (ADR-0010 Q7):** `renderer::Camera` stays pure math — it knows
`orbit(dx,dy) / zoom(delta) / pan(dx,dy)` and nothing about pixels. Pixel→radians / pixel→world
conversion is an **app/input-layer** concern, and the 0023 sign/unit tests depend on the Camera
contract staying free of it.

**M5 to decide:** the owning type name and scope (`InteractionSettings`? `EditorPreferences`?
`CameraInputConfig`?) — do **not** commit to `CameraPrefs` or any other name pre-emptively; add the
knobs in the layer that already maps gestures, and re-review before extracting a type.

---

## Acceptance

- [ ] Both items decided during M5 with the Q7 boundary preserved (Camera remains sensitivity-agnostic)
- [ ] Cursor-baseline reset semantics defined for the events ImGui actually introduces
- [ ] Sensitivity configuration owned by the app/editor input layer, not `renderer::Camera`
- [ ] 0023 camera tests (`tests/renderer/camera_test.cpp`) stay green and unchanged in spirit

**Points to ADR:** 0010 (Q7 camera purity) · 0005 (ImGui pass handoff)
