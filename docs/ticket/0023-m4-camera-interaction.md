# 0023 — M4 camera & interaction baseline

**What to build:**

`noto3d::renderer::Camera` — a pure-math orbit/zoom/pan camera (per ADR-0010 Q7) — plus the direct
GLFW→Camera wiring in the app layer. No Input abstraction, no scene/registry/ImGui coupling.

```cpp
namespace noto3d::renderer {
class Camera {
public:
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect) const;
    void orbit(float dx, float dy);  // radians per axis delta
    void zoom(float delta);
    void pan(float dx, float dy);
    // const getters: yaw/pitch/distance/target
};
}
```

## Scope

**A — Camera math (pure, headless; ADR-0010 Q7 + Amendment):**
- State: `target`, `yaw`, `pitch`, `distance`. **Spherical convention (locked)**:
  ```cpp
  offset.x = cos(pitch) * sin(yaw) * distance;
  offset.y = sin(pitch) * distance;
  offset.z = cos(pitch) * cos(yaw) * distance;
  eye = target + offset;                      // worldUp = (0,1,0)
  forward = normalize(target - eye);
  right   = normalize(cross(forward, worldUp));
  up      = cross(right, forward);
  ```
- **Initial state (locked)**: `target=(0,0,0)`, `yaw=0`, `distance=sqrt(9.25)`,
  `pitch=asin(0.5/sqrt(9.25))` ⇒ eye **exactly** `(0,0.5,3)` = M3 camera. `viewMatrix()` =
  `lookAt(eye, target, up)`; verified by equality with `glm::lookAt((0,0.5,3),(0,0,0),(0,1,0))`
  (tolerance ok) and on the yaw/pitch/distance primitives, never "looks about right".
- `projectionMatrix(aspect)`: FOV 45°, near 0.1, far 100 (clipping planes) — recomputed per frame
  with the framebuffer aspect.
- `orbit(dx,dy)`: `yaw += dx`; `pitch += dy`, **pitch clamped ±89°** (no pole flip). Sign
  contract: yaw grows with dx, pitch grows with dy.
- `zoom(delta)`: `distance = clamp(distance - delta, 0.5f, 20.0f)` — positive delta zooms in
  (distance decreases). Range `[0.5, 20.0]` is an interaction range independent of near/far.
- `pan(dx,dy)`: `target += right * dx + up * dy` — inputs are **world-space displacement units**
  along camera-local axes. The pan basis is exactly the orthonormal `right`/`up` derived **from
  the current view** (forward × worldUp) in the spherical block above — **not** world X/Y axes,
  and `up` is the orthonormalized camera-up, not worldUp. Positive dx → camera-local right,
  positive dy → camera-local up. Pan mutates **only** `target` (eye = `target + spherical
  offset`); `yaw`/`pitch`/`distance` are untouched, so the basis stays fixed during a drag and
  orbit/zoom/pan remain consistent.
- No mutable-state exposure (public `float& yaw()` etc. forbidden); const getters only.

**B — interaction wiring (`main.cpp`):**
- LMB drag → `orbit(dx, dy)`; wheel → `zoom(delta)`; MMB drag → `pan(dx, dy)`; ESC → quit
  (unchanged). GLFW cursor/wheel polling + callbacks translated to camera calls in app; unit
  conversion (pixels → radians / world units) happens here, not in Camera (the Camera numeric
  sign contract above is fixed by 0023's tests; the app maps gestures to it as intended).
- Replace the current hard-coded `proj`/`view` in the render loop with
  `camera.projectionMatrix(aspect) * camera.viewMatrix()`.
- No `InputManager/InputState/InputEvent/ActionMap` (reassess at M5 under ImGui).

**C — headless tests (`tests/renderer/camera_test.cpp`):**
- Default Camera view == `glm::lookAt((0,0.5,3), (0,0,0), (0,1,0))` within tolerance.
- Initial primitives == locked initial state (`target/yaw/pitch/distance`, eye ≈ (0,0.5,3)
  elementwise).
- Projection at 45°/0.1/100 for a given aspect.
- Orbit: `+dx` increases yaw (eye moves toward +X of the initial view), `+dy` increases pitch;
  pitch clamps at ±89° (no pole flip).
- Zoom: `+delta` decreases distance, clamp at `[0.5,20]`; FOV unchanged.
- Pan: `dx/dy` move target along camera-local right/up (world units), not global X/Y.

## Acceptance

- [ ] Pure-math `renderer::Camera`, no GL/Scene/GLFW/input-abstraction dependency
- [ ] Locked spherical convention + initial yaw/pitch/distance ⇒ initial eye exactly `(0,0.5,3)`
- [ ] Initial view == M3 view (lookAt equality test); radians throughout
- [ ] orbit sign (yaw grows with dx, pitch grows with dy) / pitch clamp ±89°
- [ ] zoom sign (+delta = zoom-in) / distance clamp 0.5–20, independent of near/far
- [ ] pan in world units along camera-local right/up (positive dx/right, dy/up)
- [ ] GLFW wiring: LMB=orbit, wheel=zoom, MMB=pan, ESC=quit; no Input class introduced
- [ ] `tests/renderer/camera_test.cpp` green (dev + release), headless
- [ ] Render loop consumes `camera.projectionMatrix(aspect) * camera.viewMatrix()`

**Blocked by:** 0022 (complete)
**Status:** pending
**Points to ADR:** 0010 (Q7) · 0004 (viewport/DPI, framebuffer aspect) · 0005 (pass hygiene)
**Rolls under spec:** ADR-0010 (M4 spec embedded; execution tickets only in docs/ticket/)