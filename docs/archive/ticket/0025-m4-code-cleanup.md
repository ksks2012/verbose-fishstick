# 0025 — M4 code cleanup (narrowed scope)

**Source:** code review of tickets 0021–0024 (Sep 2026), then a strict re-review (Sep 2026) that
narrowed this ticket to work that is actually safe to do now.
**Status:** completed (Sep 2026)
**Blocked by:** none (infrastructure pass, independent of M5)

> **Scope discipline (Sep 2026 re-review).** This ticket deliberately does **not** remove public
> APIs, does **not** introduce test-only build conditionals, does **not** split libraries, and
> does **not** pre-design M5 features. Items that need an architecture decision are recorded as
> deferred notes here and split into follow-up tickets:
> - M5 input/interaction concerns → **0026** (cursor baseline semantics, camera interaction
>   configuration ownership)
> - asset CPU/GPU library split → **0027** (architecture spike)
>
> Two original claims were stale and were corrected: the `ShaderProgram` uniform cache already
> exists (0011⑩ delivered with 0012) and `loadManifestOnly()` is **not** test-only — it backs the
> production `--list-assets` diagnostic CLI (0017/ADR-0009).

> **Top-level invariant (applies to every item below).** 0025 must not change externally observable
> M4 runtime behaviour, CLI semantics, or any established architecture contract: `scene::Scene`
> stays pure data, `scene::modelMatrix` stays the single T·R·S definition, `app::collectDrawItems`
> stays an app-layer resolution seam with borrowed `DrawItem::model` pointers, `AssetRegistry`
> remains the sole asset authority, and `renderer::Camera` semantics (orbit/zoom/pan signs, clamps,
> initial view) are untouched. When an item is ambiguous, this invariant wins: leave the behaviour
> alone, do the structural move, and record the open question in the evidence.

---

## P1 — ① Extract CLI option parsing to `src/app/cli_options.{hpp,cpp}`

`main.cpp` currently holds `Options`, `parseOptions`, the window constants, `InputState`, the GLFW
callbacks and the sensitivity constants in one anonymous namespace. `Options`/`parseOptions` cannot
be verified headlessly without forking a subprocess — the CLI contract (ADR-0010 Q8) is currently
only checked through the built binary.

**Work:**
- `src/app/cli_options.hpp` — `Options` (+ a mode discriminator for default / `--asset` / `--demo`
  / `--list-assets` if that reads better than the current optional/flag pair) and parse
  declarations.
- `src/app/cli_options.cpp` — `parseOptions` with today's validation **unchanged**: validated
  `--frames=` (non-negative integer, 0011-⑨), `--asset=` with non-empty id, `--demo`,
  `--list-assets`, and the unknown-flag rejection added in 0022.
- `tests/app/cli_options_test.cpp` — headless, dependency-free, **asserts on `Options` content,
  not only on exit codes**: default; `--asset=smile|crown|sunglasses`; `--demo`; unknown asset;
  `--asset` + `--demo`; unknown flag; malformed `--frames=abc` / negative `--frames=`.
- `main.cpp` keeps only GLFW lifecycle, callbacks, `InputState` and the sensitivity constants
  (see the note below on constant placement).

**Boundary note:** window geometry/title/version constants may stay in `main.cpp` or move to a small
app-config header; at this size either is acceptable — do not create a new module for them.

**Invariant:** CLI behaviour is byte-for-byte identical after the move; the 0024 matrix
(exit codes + `M4_INFO` lines + titles) must stay green.

**Points to ADR:** 0008 (module layout `src/app`, dependency-free headless tests) · 0010 (Q8
regression contract) · 0009 (M3 observability preserved)

---

## P2 — ④ Flatten the GL lifecycle scope in `run()`

`run()` wraps ~200 lines in a bare `{ ... }` block whose only purpose is to guarantee that GL-owning
objects (shader, registry, renderer, scene) are destroyed **while the context is still current**.

**Work:** extract that block into a helper returning the exit code —
`static int renderScene(GLFWwindow* window, const Options& options)` — so `run()` reads as: GLFW
init → window creation → `renderScene()` → window/context destruction. If the implementation needs
further parameters, derive them from the existing code rather than inventing a new contract; the
GL-owning objects (shader, registry, renderer, scene) are created **inside** the helper, not passed
in. Preserve destruction ordering exactly; this is a readability change, not a behaviour change.

**Depends on:** ① (the helper takes `const Options&` from the extracted header).

**Points to ADR:** 0008 · 0012 (the GL-scope lifetime rule the extraction must preserve)

---

## P2 (verification-only) — ② Verify the uniform-location cache

**No code change is planned.** The lazy per-name cache already exists
(`m_uniformCache` in `src/renderer/shader_program.hpp`, lookup/store in `shader_program.cpp`,
cleared whenever the program handle is invalidated), delivered by 0011⑩ with 0012.

> **Correction during verification (Sep 2026).** There is **no** `ShaderProgram::reload()` method —
> the cache has no per-program "reload" path to clear. Invariant 3 is therefore verified in its
> actual, stronger form: the cache never outlives its GL program (see evidence).

**Work:** confirm the cache is still present and correct. Record evidence for exactly these three
invariants:
1. cache hit → **no** `glGetUniformLocation` call;
2. cache miss → query once, then store;
3. `reload()` → cache cleared. (No `reload()` method exists; verified as: the cache is cleared
   exactly when the program handle is invalidated — `release()`, and it is moved together with the
   handle — see evidence.)

Code evidence is sufficient; do **not** build a new GL-context test for this item. If — and only if
— a defect is found, fix it and say so in the ticket evidence. Do not "improve" a working cache to
have something to show for this item.

**Tracked by:** 0011⑩ / 0012 (complete).

---

## Deferred decision — ③ Audit the `loadManifestOnly()` test seam (do not remove)

`loadManifestOnly()` (CPU-only manifest parse, no GL upload) is a **public, production-used** API:
`main.cpp::listAssetsCommand()` uses it for the pre-GL `--list-assets` diagnostic CLI (0017,
ADR-0009). AssetRegistry's own tests are *supposed* to exercise `manifest → AssetRegistry`, so the
draw-list tests' injectable-`Resolver` pattern is not the same seam and does not transfer.

**Record (documentation only — no judgement calls here):**
1. Current role: the production entry point behind `--list-assets`, plus the CPU-only path used by
   asset tests; the reason it exists (AssetRegistry's GL coupling, see 005 / 0027).
2. Current constraints: it is a public production API; the draw-list tests' injectable-`Resolver`
   pattern is a **different layer** and does not replace it (asset tests are meant to exercise
   `manifest → AssetRegistry`).
3. Which question belongs to 0027: whether the CPU/GPU coupling should be restructured, and whether
   `loadManifestOnly()` survives, becomes the `asset_cpu` entry point, or is superseded.

**Explicitly out of scope:** deciding or redesigning the future seam here, removing the method,
moving it to a `testing` namespace, adding `#ifdef` gating (production `--list-assets` calls it, and
a config-dependent public interface is a worse contract), or refactoring manifest parsing.
**Output:** current state + constraints + the open question handed to 0027 — no architecture
decision, no ADR draft.

**Points to ADR:** 0007 (AssetRegistry is the sole asset authority) · 0009 (0017 pre-GL
`--list-assets`)

---

## Deferred — ⑤ `noto3d_asset` → `noto3d_renderer` PUBLIC coupling (architecture note only)

`SubMesh::mesh` is a `renderer::Mesh`, so `noto3d_asset` links `noto3d_renderer` PUBLIC and any
manifest-only consumer transitively pulls glad + GL headers. Splitting `asset_cpu` / `asset_gpu`
is an **architecture refactor**, not cleanup: it needs an ownership decision for
`CanonicalShape` / CPU `Material` / manifest representation vs `Mesh`/`SubMesh`/`Material`/
`Texture`/`Model`, plus a decision on where GL upload happens.

**No implementation in this ticket.** Recorded in **0027** (spike) for M5+.

---

## P2 (verification-only) — ⑧ Verify production/test target boundaries

Before any dependency refactor, confirm the CMake graph matches intent:

- `noto3d_scene` → glm only; no asset / renderer / GL.
- `noto3d_app` → scene + app seam (draw-list); no GL window/GLFW dependency in the seam library.
- `noto3d_asset` → `noto3d_renderer` (GL) — currently PUBLIC; **expected today, and the thing
  0027 will address**. Record which test targets actually need the GL link
  (`tests/asset/` does today; `tests/scene/` and `tests/app/` should not).
- `tests/renderer/camera_test.cpp` → must stay headless (no GL context needed for the math).

**Work:** read `CMakeLists.txt`, record the actual graph in the ticket evidence, fix only clear
oversights (e.g. a target linking GL unnecessarily). No library splitting here.

**Boundary:** do **not** change `noto3d_asset → noto3d_renderer` link visibility (PUBLIC → PRIVATE or
vice versa) to make the graph look tidier — that decision belongs to 0027 and include propagation
depends on it.

---

## Acceptance

- [x] ① `src/app/cli_options.{hpp,cpp}` extracted; `tests/app/cli_options_test.cpp` asserts
      `Options` **content** (default / `--asset` / `--demo` / unknown asset / `--asset`+`--demo` /
      unknown flag / malformed `--frames`) headlessly
- [x] ① CLI behaviour unchanged: 0024 matrix (exit codes, `M4_INFO` lines, titles) still green on
      dev + release
- [x] ④ `renderScene()` extraction preserves GL-scope destruction ordering; ctest + matrix green
- [x] ② uniform-location cache verified against three invariants (hit → no query, miss → query +
      store, reload → cleared); no gratuitous change
- [x] ③ current role + constraints + the 0027 hand-off question recorded; **no** architecture
      judgement, no removal, no `#ifdef` gating
- [x] ⑤ recorded as deferred → 0027; no implementation
- [x] ⑧ actual CMake target graph recorded; only clear oversights fixed; `noto3d_asset →
      noto3d_renderer` visibility untouched
- [x] Global invariant held: no externally observable M4 behaviour or established architecture
      contract changed
- [x] Follow-ups split out: 0026 (M5 input/interaction), 0027 (asset CPU/GPU architecture spike)
- [x] CHANGELOG + CONTEXT updated; no M5 functionality pulled into this ticket

**Rolls under spec:** infrastructure cleanup — not a feature spec; bounded by ADR-0008 (module
boundaries) and ADR-0010 Q8 (CLI regression contract).

---

## Evidence (Sep 2026)

### ① CLI parsing extraction

- New `src/app/cli_options.hpp` / `src/app/cli_options.cpp` own `noto3d::app::Options`
  (`frameLimit`, optional `assetId`, `demo`, `listAssets`) and `parseOptions()`, moved from
  `main.cpp`'s anonymous namespace. Validation is unchanged: `--frames=` non-negative integer,
  `--asset=` non-empty id, `--demo`, `--list-assets`, unknown-flag rejection (0022).
- The header documents the layering chosen after review: `parseOptions()` is a **pure syntax
  contract**; `--asset` + `--demo` mutual exclusion stays in `main()` (it must reject **pre-GL** so
  the 0024 regression is testable without a display), and unknown-asset validation stays in the
  registry layer. The ticket's "mode discriminator" was judged unnecessary — the existing
  optional/flag pair is preserved, so no new contract was invented.
- Window geometry / title / version-hint constants deliberately stayed in `main.cpp` (GLFW concern;
  the ticket allows either, but forbids a new module).
- `tests/app/cli_options_test.cpp`: 12 blocks / **40 checks** asserting `Options` **content**, not
  exit codes: default; `--asset=smile|crown|sunglasses`; `--demo`; unknown asset; `--asset` +
  `--demo`; unknown flag; `--frames=abc`; `--frames=-1`; `--frames=0`; `--list-assets`;
  `--asset=`; extra positional. New target `noto3d-cli-options-tests`, new CTest
  `app-cli-parse-contract` (linked to `noto3d_app` + `noto3d_core`; no GL context is created).
- TDD evidence: red first (the suite could not even configure without the new source: `No SOURCES
  given to target: noto3d_app` / `Cannot find source file: src/app/cli_options.cpp`), then the first
  green attempt failed 10 checks with `check(0, "argc>=1", false)` because the test helper dropped
  `argv[0]` — the assertions were live before the implementation existed.
- Suite count: 14 → **15**. `ctest` 15/15 on dev **and** release.

### ④ `renderScene()` extraction

- `int renderScene(GLFWwindow* window, const noto3d::app::Options& opts)` now owns the whole render
  path; `run()` reads GLFW init → window/context creation → capability gate → `renderScene()` →
  `glfwDestroyWindow` → `glfwTerminate`. The bare `{ ... }` scope block and its extra indentation
  level are gone — the function body **is** the scope, so the GL-scope lifetime rule (ticket
  0012) is now
  structural rather than conventional.
- GL-owning objects (`ShaderProgram`, `AssetRegistry` → `Mesh`/`Texture`, `Renderer`, `Camera`) are
  locals of `renderScene()` (created inside, not passed in) and are destroyed on return while the
  context is still current.
- `glfwSetWindowUserPointer(window, &inputState)` stays with the input state it owns; `M0_OK`,
  `M4_INFO`, `M1_DONE` and `M2_OK` logging and the `--frames` loop are unchanged.
- Verified unchanged on **dev + release**: 5 valid invocations → exit 0; 6 invalid → exit 1
  (`--asset=nope`, `--asset=smile --demo`, `--bogus`, `--frames=abc`, `--asset=`,
  `--frames=-1`); log order `M0_OK` → `M4_INFO rendering scene 'm4_vertical_scene' entities=3` →
  `M1_DONE rendered 1 frames`; `--demo` → `M2_OK ... (0009 E2E)` + `entities=1`; xprop `WM_NAME`
  for all three modes (`m4_vertical_scene | noto3d (M4)`, `smile [placeholder] | noto3d (M4)`,
  `demo_checker [placeholder] | noto3d (M4)`).

### ② Uniform-location cache — verified, no change

Code evidence (`src/renderer/shader_program.cpp`):

1. **hit → no query** — `uniformLocation()` returns the cached `GLint` at lines 108–110, before the
   `glGetUniformLocation` call on line 111.
2. **miss → query once + store** — line 111 queries, line 112 `emplace`s into
   `m_uniformCache`.
3. **cache never outlives its program** — there is no `reload()`; the cache is cleared exactly when
   the handle dies: `release()` (132–138) does `glDeleteProgram` **and** `m_uniformCache.clear()`,
   and is reached from the destructor (82–84) and move-assignment (92–100); move-construction
   (86–90) moves the cache together with the handle, and `fromFiles()` (47, 76) always builds a
   fresh object. The `mutable` cache (hpp:45) is what makes the `const` setters able to populate it.

No code change, no new GL test — per the ticket, a working cache is not "improved" to produce a
diff. No defect found.

### ③ `loadManifestOnly()` — current state and 0027 hand-off (documentation only)

- **Current role:** the production entry point behind the pre-GL `--list-assets` diagnostic CLI
  (`main.cpp::listAssetsCommand()`, 0017 / ADR-0009) and the CPU-only manifest path used by the
  asset tests. It is step 2 of the three-phase load contract documented in
  `src/asset/asset_registry.hpp`. CTest `cli-list-assets` depends on it.
- **Why it exists:** `AssetRegistry` is GL-coupled (005 / 0027), so manifest parsing cannot be
  reached without pulling the renderer.
- **Current constraints:** public production API — removing it, moving it to a `testing`
  namespace, or `#ifdef NOTO3D_TESTING`-gating it were all rejected (a config-dependent public
  interface is a worse contract, and the production call site would break). The draw-list tests'
  injectable-`Resolver` pattern is a **different layer** and does not replace it: asset tests are
  meant to exercise `manifest → AssetRegistry`.
- **Hand-off question for 0027:** should the CPU/GPU coupling be restructured, and does
  `loadManifestOnly()` survive, become the `asset_cpu` entry point, or get superseded by a
  different split? No judgement made here; no ADR draft, no refactor.

### ⑤ Deferred, no implementation

`noto3d_asset` → `noto3d_renderer` stays **PUBLIC**; the CPU/GPU split lives in
`docs/ticket/0027-asset-cpu-gpu-boundary.md`.

### ⑧ Actual CMake target graph (recorded)

| Target | Links |
|---|---|
| `noto3d_core` | PUBLIC `spdlog`; **glad headers only** (no link) — `src/core/gl_capability.hpp` includes `<glad/gl.h>` for the `GL_CONTEXT_*` enum bits and calls no GL symbol |
| `noto3d_renderer` | PUBLIC `noto3d_core`, `glm`, `glad`; PRIVATE `stb_header` |
| `noto3d_asset` | PUBLIC `noto3d_renderer`, `nlohmann_json`; PRIVATE `tinyobjloader` — GL reach expected today (0027) |
| `noto3d_scene` | `glm` only — no asset / renderer / GL, as intended |
| `noto3d_app` | PUBLIC `noto3d_scene`, `noto3d_renderer`; **no GLFW** — the seam library is window-free; the `renderer` reach is the `DrawItem::model` type dependency (`const renderer::Model*`), not a GL context need |
| `noto3d-editor` | PRIVATE `glfw`, `glad`, `glm`, `imgui`, `noto3d_app`, `noto3d_asset` |

Test targets: `core`→`noto3d_core`; `registry`→`noto3d_asset` (GL transitively, expected today);
`manifest`→`noto3d_core` + `nlohmann_json` (**no GL**); `renderer-tests` / `material` / `camera` →
`noto3d_renderer`; `scene-tests`→`noto3d_scene` + `noto3d_core` (**no GL**); `app-tests` /
`cli-options-tests`→`noto3d_app` + `noto3d_core`. `tests/renderer/camera_test.cpp` stays headless
(only `Camera` + `glm` math — no context).

**Oversights found: none requiring a change.** Three observations recorded instead of acted on:
(a) `noto3d_core` exposes glad's include dir — *correct as-is*, `gl_capability.hpp` needs those
enums; (b) `noto3d_app` reaches GL headers via `renderer::Model*` — inherent to the 0021 seam
contract; (c) `noto3d-cli-options-tests` links `noto3d_app` to test the real library artifact
(same pattern as `draw_list_test`) although the parse TU itself needs only `noto3d_core` — no
coverage gap, since the `cli-*` CTest matrix exercises the real binary.

### Global invariant

Unchanged: exit-code matrix, `M4_INFO`/`M2_OK`/`M1_DONE` log content and order, window titles,
`scene::Scene` purity, single `scene::modelMatrix`, `app::collectDrawItems` as the sole render
seam with borrowed model pointers, `AssetRegistry` authority, `Camera` semantics, and all M4
contracts recorded in ADR-0010. The only behavioural change in this ticket is the one 0022
introduced and 0024 failed to cover: an unknown flag now exits 1 instead of being ignored.

### Code review (dual-axis: standards + spec)

Both axes ran against `HEAD` with the smell baseline; **no hard violation** of a documented
standard, no scope creep, and the top-level invariant held (no diff under `src/scene`,
`src/asset`, `src/renderer`, `src/app/draw_list.*`, `src/renderer/camera.*`).

Fixed from the findings:
- `src/app/main.cpp`: includes made dead by the extraction (`<cstring>`, `<optional>`) and two
  already-dead ones (`<cstdint>`, `<stdexcept>`) removed.
- `noto3d-cli-options-tests` no longer links `noto3d_app`; it compiles `src/app/cli_options.cpp`
  and links `noto3d_core` only, with the `src/app` include dir added explicitly. ⑧ required
  `tests/app/` **not** to need the GL link, and the suite now verifies that in the build graph
  itself (no glad linkage, no GL symbols in the binary). The built editor stays covered
  end-to-end by the `cli-*` CTest matrix, so nothing lost.
- `tests/app/cli_options_test.cpp` uses the sibling tests' `using namespace noto3d::app;` style.
- Dangling `ADR-0012` citation removed (there is no ADR-0012; it is archived **ticket** 0012) from
  this ticket, `docs/CHANGELOG.md`, `docs/CONTEXT.md` and the `main.cpp` comment.

Recorded, deliberately **not** changed (outside this ticket's scope or under the invariant):
- The `--asset` + `--demo` cascade and the `Resolver` lambda appear twice inside
  `renderScene()` (verbatim 0022 code, now moved). Deduplicating it would edit M4 contract code
  that 0024 verified, so it is left for a dedicated M4-internals ticket.
- `frameLimit == -1` doubles as "unlimited" and as the parse-failure marker, and `parseOptions`
  takes `bool(..., Options& out)`, leaving `out` unspecified on failure. Both are inherited shape
  that the ticket explicitly kept (no mode enum, no signature redesign) — flagging for a future
  CLI-typing ticket.
- `run()` still contains the GL capability gate and the `NOTO3D_DEBUG_GL` block between window
  creation and `renderScene()`. The ticket sketched a shorter shape, but moving them would
  reorder `glfwSetWindowUserPointer`/logging relative to the gate — the invariant says leave
  behaviour alone.
- `tests/app/draw_list_test.cpp` keeps its heavier `noto3d_app` link (0021 scope, 0027 territory).
- Two raised findings were checked and found not to be defects: the test helper was already the
  house 2-arg `check(bool, const char*)` shape used by all seven suites, and unknown-asset
  **content** was already asserted (Test 7: `--asset=nope` → `assetId == "nope"`, parse succeeds
  because resolution is the registry's job).
