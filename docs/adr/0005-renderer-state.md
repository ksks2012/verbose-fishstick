# ADR-0005 — Renderer state contract

Status: Accepted

## Context
Per-draw-call state thrash and late-found baseline issues (culling/winding, blending model,
sRGB clear-color) cause "M1 runs, M3 looks wrong" rework. Winding of future official Noto OBJ
is unknown (recorded by the M0.5 contract).

## Decision
- Depth: enabled, `GL_LESS`, depth mask on, `clearDepth 1.0`.
- Culling: enabled, `GL_BACK`, front face `GL_CCW`. Per-material `cullEnabled` flag (default true)
  is the escape hatch. `recorded_winding` is metadata only — it never flips GL state.
- Blending: enabled, straight alpha (`GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA`).
- Clear color: owned by the renderer, authored in **linear RGB**, default `(0.12, 0.12, 0.14)`
  (sRGB framebuffer converts on output — never guess linear vs sRGB at call sites).
- Placeholder geometry is authored CCW-outward, Y-up.
- MSAA: `GL_MULTISAMPLE` enabled.
- Pass hygiene: renderer establishes canonical 3D state at the start of its pass
  (`Begin3DPass`/`End3DPass`); the ImGui overlay owns and restores its own state.
  States are set per-pass, NOT re-set per draw call.
- Known MVP limitation: no transparent-object depth sorting; overlapping transparent objects may
  artifact. Documented as limitation, not backlog.

## Consequences
- Consistent baseline from M1 onward; asset-specific issues handled via material flags, not
  renderer changes.

## Amendment — viewport / scissor / wheel ownership (2026-09-28, ADR-0011 Q0/Q9/Q10)

M5 introduces a docked editor, so the 3D view is no longer identical to the framebuffer. Pass
hygiene above is unchanged (renderer still establishes canonical 3D state; ImGui overlay still
owns and restores its own state). What is added is **who sets the viewport rect, and what the
clear is confined to**:

- **The application layer establishes the viewport** from the frame's `ViewportRect` (framebuffer
  pixels, app-layer plain data) **before** calling `Begin3DPass()`. `Begin3DPass()` remains
  UI-agnostic and takes no rect at M5 — it keeps owning state, not geometry.
- **The scissor box follows the viewport rect**, so the pass' `glClear` is confined to the 3D
  view instead of clearing the whole framebuffer. Viewport and scissor are set together as one
  contract; leaving the scissor at the default full-framebuffer box is a defect, not a
  stylistic choice.
- **Per-frame re-establishment is required**, because the ImGui OpenGL3 backend saves and restores
  viewport/scissor state around its own rendering. No manual save/restore handshake is needed in
  app code, and none should be added.
- **Wheel ownership follows the pass contract.** The scroll delta consumed for camera zoom is
  gated by viewport containment/ownership (ADR-0011 Q9/Q10); the viewport window is created with
  `NoScrollbar | NoScrollWithMouse` so the wheel is not consumed by ImGui scrolling. The wheel is
  not a special case exempt from input ownership.
- Projection aspect is derived from the same `ViewportRect` (see the ADR-0004 amendment).

Consequence: `Renderer` gains no new parameters, no ImGui knowledge, and no geometry concept.