# 0007 — M1 pass hygiene: Begin3DPass/End3DPass (ADR-0005)

**What to build:**

Canonical 3D renderer state, established once per pass (not per draw call). ADR-0005 is the
contract: the renderer owns the 3D pass state; per-material flags are the only per-draw variance.

Contract (ADR-0005):
- Depth: enabled, `GL_LESS`, depth mask on, `clearDepth 1.0`.
- Culling: enabled, `GL_BACK`, front face `GL_CCW`; `Material::cullEnabled` (default true) is the
  per-draw escape hatch — `recorded_winding` is metadata only and never flips GL state.
- Blending: enabled, straight alpha (`GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA`).
- Clear color owned by the renderer, authored in **linear RGB** `(0.12, 0.12, 0.14)`; converting
  linear↔sRGB is never a call-site concern.
- `GL_MULTISAMPLE` enabled (MSAA 4x, ADR-0004).
- Pass ownership: `Begin3DPass`/`End3DPass` establish and restore canonical state; states are set
  per-pass, NOT re-set per draw call.
- The ImGui overlay owns and restores its own state — deferred to M5 (ImGui editor); at M1 the
  overlay pass does not exist yet and is documented, not implemented.

Known MVP limitation (documented, not backlog): no transparent-object depth sorting.

**Blocked by:** 0006

**Status:** ready-for-agent

- [ ] `Begin3DPass`/`End3DPass` set/restore the full ADR-0005 state block (depth/cull/blend/
      multisample/linear clear color)
- [ ] `Material::cullEnabled` honored per draw; `recorded_winding` never flips GL state
- [ ] States set once per pass, not per draw call
- [ ] ImGui-pass state ownership documented as an M5 responsibility (no overlay code at M1)

**Points to ADR:** 0005 (renderer state contract)