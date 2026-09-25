# 0012 — M1 app/renderer separation: extract Renderer + implement Begin3DPass (ADR-0005/0008)

**What to build:**

`src/app/main.cpp` currently owns the window lifecycle, GL context, capability validation, M1
asset setup (inline shader + cube mesh + material), and the render loop with inline draw calls.
ADR-0008 defines `src/{app,renderer}` as separate module boundaries; ADR-0005 defines pass-level
state ownership. Neither contract is implemented yet — everything is inlined in `main.cpp`.

This ticket decomposes `main.cpp` into two responsibilities and directly implements ticket 0007:

1. **`noto3d::renderer::Renderer`** (`src/renderer/renderer.hpp/.cpp`) — owns:
   - `Begin3DPass()` / `End3DPass()` per ADR-0005: establishes canonical depth/cull/blend/
     multisample/clear-color state once per pass; no per-draw-call re-application.
   - Per-draw submission (`submit(Mesh&, Material&, ShaderProgram&, mat4 mvp)`).
   - `Material::cullEnabled` honored per draw; `recorded_winding` never flips GL state.
   - Linear-space clear color ownership (`(0.12, 0.12, 0.14)`).

2. **`src/app/main.cpp` slimming** — retains only:
   - Window + context lifecycle (GLFW init/terminate, `glfwCreateWindow`).
   - GL context validation (`supportsRequiredContext`).
   - Calling `Renderer::Begin3DPass()` / `End3DPass()` in the render loop.
   - Asset/shader setup delegated to helper functions or thin wrappers.

Contract constraints:
- States set **once per pass**, not per draw call (ADR-0005).
- ImGui-pass state ownership is an M5 concern; `End3DPass()` at M1 simply signals end-of-pass
  with no snapshot/restore — documented limitation (ADR-0005, no transparent-depth sort).
- No snapshot/restore machinery at M1.
- Existing `TODO(0007)` cull-state per-draw workaround in `main.cpp` is removed.

**Blocked by:** 0007 (supersedes it), 0009

**Status:** ready-for-agent

- [ ] `noto3d::renderer::Renderer` in `src/renderer/`; `Begin3DPass()` sets full ADR-0005 state block
- [ ] `End3DPass()` terminates pass (no GL state restore at M1)
- [ ] `submit()` (or equivalent) handles `cullEnabled` per draw, clears `TODO(0007)` workaround
- [ ] `main.cpp` reduced to window/context lifecycle + calls into Renderer; no inline GL state
- [ ] All existing tests green; `noto3d-editor --frames=5` still prints `M1_OK`/`M1_DONE`
- [ ] ADR-0005 ImGui limitation documented in code comment (M5 responsibility)

**Points to ADR:** 0005 (renderer state contract), 0008 (module boundaries: src/app vs src/renderer)
**Supersedes / closes:** ticket 0007
