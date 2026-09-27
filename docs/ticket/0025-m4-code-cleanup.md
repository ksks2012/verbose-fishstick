# 0025 — M4 code cleanup and structural refactoring

**Source:** Code review of tickets 0021–0024 (Sep 2026)
**Status:** ready-for-agent
**Blocked by:** none (infrastructure pass, independent of M5)

---

## ① Extract CLI option parsing to `src/app/cli_options.hpp/.cpp`

`src/app/main.cpp` currently contains `Options`, `parseOptions`, `kWindowTitle`, `kInitialW/H`,
`InputState`, `scrollCallback`, and all sensitivity constants in an anonymous namespace.  The
`parseOptions` and `Options` types are not testable headlessly without forking a subprocess.

**Suggested fix:** extract to `src/app/cli_options.hpp` (Options struct + constants) +
`src/app/cli_options.cpp` (parseOptions, validation).  Add `tests/app/cli_options_test.cpp` to
test parse behaviour in-process without a binary invocation.

**Points to ADR:** 0008 (module boundaries: src/app), 0010 (Q8 regression contract)

---

## ② Cache uniform locations in `ShaderProgram`

`src/renderer/shader_program.cpp` — `setMat4`, `setVec4`, `setFloat`, `setInt` each call
`glGetUniformLocation` on every invocation.  At M4 with three entities and one shader this is
immaterial, but the pattern degrades as the draw list grows.

**Suggested fix:** lazy cache of `GLint` per name inside `ShaderProgram` using an
`std::unordered_map<std::string_view, GLint>`; populate on first query.

**Tracked in:** 0011 (⑩); this entry confirms it survived the M4 draw-list multiplication.

---

## ③ `loadManifestOnly()` is test-only API in production header

`src/asset/asset_registry.hpp` exposes `loadManifestOnly()` as a public method; the method exists
solely to support headless tests (no GL upload).  The production interface is `load()`.

**Option A:** Remove `loadManifestOnly()` and make tests use a fake `Resolver` pattern (already
done in `tests/app/` via injectable `Resolver`; apply the same discipline to asset tests).

**Option B:** Retain `loadManifestOnly()` but move it to a `testing` subnamespace or gate it
behind `#ifdef NOTO3D_TESTING`.

---

## ④ `main.cpp` scene-scope brace depth

The GL-owning scope `{ ... }` inside `run()` adds one level of indentation to the entire body
of the function.  At M4 the nested content is ~200 lines.  A named helper function
`runInScope(window, opts, ...)` that returns an `int` would flatten the nesting.

**Suggested fix:** extract the scene scope into `static int renderScene(GLFWwindow*, const Options&)`
and call it from `run()`.  `run()` retains only GLFW lifecycle + the call.

---

## ⑤ `noto3d_asset` links `noto3d_renderer` PUBLIC (GL exposed to asset consumers)

`noto3d_asset` has `noto3d_renderer` as a PUBLIC dependency because `SubMesh::mesh` is a
`renderer::Mesh` (requires glad headers).  Consumers that only need manifest parsing (e.g.,
tests using `loadManifestOnly`) transitively get glad + GL.

**Option:** Separate the CPU-side types (`CanonicalShape`, CPU-only `Material`, etc.) into a
`noto3d_asset_cpu` library with no GL dependency; the GL upload step lives in `noto3d_asset_gpu`
that depends on both.  This is a non-trivial refactor; defer to M5 or a dedicated spike.

---

## ⑥ `InputState` lifetime not explicitly reset on reload

`InputState::firstMove` is `true` at construction but never reset if the scene is reloaded
(future `AssetRegistry::reload()` call without restarting).  Currently harmless (no reload path
exists at M4) but worth noting for M5 when ImGui may trigger reloads mid-session.

---

## ⑦ Camera sensitivity constants are not runtime-configurable

`kOrbitSens`, `kPanSens`, `kZoomSens` in `main.cpp` are `constexpr float`.  At M5 these should
be ImGui-adjustable preferences.  Capture them in a `CameraPrefs` struct that M5 can expose.

---

**Urgency ranking (suggested order):**
1. ① CLI option extraction — unblocks headless parse tests; low risk
2. ② Uniform caching — performance; low risk
3. ④ Scene-scope flattening — readability; medium risk
4. ③ loadManifestOnly API — design; medium effort
5. ⑤ Asset/renderer coupling — design; high effort, M5+
6. ⑥⑦ M5-era concerns — defer
