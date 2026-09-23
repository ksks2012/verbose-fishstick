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