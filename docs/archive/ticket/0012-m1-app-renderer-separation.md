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

**Blocked by:** 0007 (supersedes it). 0009 is unblocked for 0012: core is committed; the asset
registry itself is not yet consumed by the app — wire the registry here (which also closes
0009's manual E2E demo-verification gate).

**Status:** completed

- [x] `noto3d::renderer::Renderer` in `src/renderer/`; `Begin3DPass()` sets full ADR-0005 state block
- [x] `End3DPass()` terminates pass (no GL state restore at M1)
- [x] `submit()` (or equivalent) handles `cullEnabled` per draw, clears `TODO(0007)` workaround
- [x] `main.cpp` reduced to window/context lifecycle + calls into Renderer; no inline GL state
- [x] All existing tests green; `noto3d-editor --frames=5` still prints `M1_OK`/`M1_DONE`
- [x] ADR-0005 ImGui limitation documented in code comment (M5 responsibility)

**Folds in ticket 0011 (M1 code-quality deferred findings):**
- [x] ⑦ `NOTO3D_SHADER_DIR` fallback cwd dependency → replaced with
      `#error "NOTO3D_SHADER_DIR must be defined by the build system"` (CMake defines it)
- [x] ⑧ `std::optional<ShaderProgram>/<Mesh>` wrappers disappear (Renderer encapsulates setup);
      inline `kCubeVerts`/`kCubeIdx` fixture removed entirely; render loop submits registry
      models with no `operator->` ceremony
- [x] ⑨ `--frames=abc` silent 0 (false-green) → `std::stoi` + full-token validation;
      non-numeric/negative prints an error and exits non-zero (verified exit=1)
- [x] ⑩ uniform-location caching in `ShaderProgram` (`std::unordered_map<std::string,
      GLint>`, lazy per-location cache; first query calls GL, later frames reuse)

**Registry integration (deferred sub-scope) — done:**
- `noto3d-editor` loads `assets/manifest.json` via `AssetRegistry`, renders the selected
  manifest asset (default first renderable; `--asset=<id>` overrides) and optionally the
  0014 demo fixture (`--demo`) through the new standalone path
  (`loadModelShapesOnly`/`loadModelFile`/`externalModel`).
- This **closes 0009's manual E2E demo-verification gate**: `--demo --frames=N` uploads the
  `demo_checker.png` as `GL_SRGB8_ALPHA8`, wires `diffuseTextureRef → texture` and prints
  `M2_OK ... with diffuse texture (0009 E2E)` (verified dev + release).
- `--asset=`/`--demo` CLI; GL-owning objects scoped so they are destroyed while the context
  is current (fixes teardown-after-`glfwTerminate` GL deletion, caught by ASan).

**Points to ADR:** 0005 (renderer state contract), 0008 (module boundaries: src/app vs src/renderer)
**Supersedes / closes:** ticket 0007 **and folds in ticket 0011**
