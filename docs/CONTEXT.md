# CONTEXT.md — current implementation truth (consolidates Accepted ADRs)

Document hierarchy:
- **CONTEXT.md = current implementation truth.** ADR files = historical decision records and
  rationale. If CONTEXT.md and an ADR disagree, the newer accepted decision supersedes the older
  one, and **both must be updated in the same change**.
- **DEFINITION.md = canonical meaning of project nouns** (glossary); see `docs/DEFINITION.md`.
- **CHANGELOG.md** = version history / milestone summaries; single location `docs/CHANGELOG.md`
  (no root `CHANGELOG.md`).

## Project
C++/OpenGL editor MVP for Noto 3D-style emoji composition ("emoji_editor").

## Current milestone
M4 Scene Composition & Transform **complete** (0020–0024) and its post-close **0025 code-cleanup
pass complete** (0020–0025 archived). **M5 ImGui Editor has an accepted architecture (ADR-0011,
grilling Q0–Q14) and its execution tickets are written (0028–0034; 0028 + 0029 implemented, manual
evidence pending)**. M0
(Environment/build),
M0.5 (Asset contract spike), M1 (minimal renderer + pass hygiene), M2 (0008 → 0014 → 0009 → 0013 +
0012/0011/0010) and M3 (0015 → 0016 → 0017 → 0018) are done and verified. Completed tickets are
archived at `docs/archive/ticket/` (all 25). Working tree state:

- `noto3d-editor` consumes the AssetRegistry: `--asset=<id>` (default first renderable manifest
  asset) and `--demo` (0014 fixture) render through the registry; 0009's E2E gate is closed
  (`M2_OK ... with diffuse texture (0009 E2E)`, dev + release).
- M3 observability: every selection prints `M3_INFO selected asset '<id>' geometry=<source>
  reference=<path>`; window title is `<id> [placeholder] | noto3d (M3)`; `--list-assets` is a
  pre-GL diagnostic CLI. M3 fixture frozen by `manifest_integrity_test` (ctest 5/5) and
  human-verified per category (0018).
- M4 delivered (ADR-0010, grilling Q1–Q8): pure-data Scene (EntityId monotonic/never-reused,
  world-space Transform·Matrix via `scene::modelMatrix`), app-level draw-list seam
  (`app::collectDrawItems`, unresolved → skip, empty Resolver ⇒ `std::logic_error`), CLI→Scene
  entry model (default `m4_vertical_scene`, `--asset` single-entity, `--demo` demo-only fixture
  scene, `--asset`+`--demo` → exit 1, no `--scene=`, invalid flag → exit 1),
  pure-math orbit/zoom/pan Camera (spherical convention locked; initial eye exactly `(0,0.5,3)`).
  Closed by an automated CLI/`M4_INFO`/CTest matrix (8/8 dev + release) + xprop title evidence +
  user-confirmed manual smoke; no readback/similarity. CLI→Scene render path is exclusively via
  `collectDrawItems()` — no direct `registry.model() → submit()` bypass.
- 0011 folded items done: ⑦ `#error` shader-dir, ⑧ no inline cube / no `optional` ceremony,
  ⑨ validated `--frames=` (non-numeric/negative exits non-zero), ⑩ uniform-location cache.
- 0010 done: all six Git deps pinned by immutable SHA (each resolves to its release tag);
  no archive sources → `URL_HASH` n/a; dev + release configure/build + `ctest` green.

Next on the roadmap: **M5 ImGui Editor**. Its architecture is locked in **ADR-0011** (docked
layout + explicit framebuffer-space `ViewportRect` driving aspect/cursor/`glViewport`/scissor;
app-layer `Editor` **owns the `Scene` by value** and is the only mutation route, so the UI
structurally cannot get a mutable `Scene&` or `Entity*`; ImGui-free editor logic in `noto3d_app`
so it stays headless-testable; input ownership gate = `viewportHovered && !IsAnyItemHovered() &&
!IsAnyItemActive() && !globalModalCapture`, with the wheel under the same gate —
**`!io.WantCaptureMouse` must never be used as the viewport camera gate**, because the viewport is
itself an ImGui window; transform editing is inspector-numeric in degrees while the `Scene` keeps
radians YXZ). Named exclusions: undo/redo, gizmos/direct manipulation, and the asset CPU/GPU
refactor. ADR-0004 and ADR-0005 were amended in the same change; ADR-0010 is unchanged.
Per DOCUMENTATION-LIFECYCLE, specs live at
the ADR layer: the M3 spec is embedded in **ADR-0009**, the M4 spec in **ADR-0010**, the M5 spec
in **ADR-0011**; `docs/ticket/`
holds execution tickets only (0026 is M5-active and blocked by the first slice's input gate; 0027
is an M6-preceding gate), plus completed
0020–0025 archived under accepted ADRs.
M0 scope, user stories and acceptance matrix: `docs/archive/ticket/0001-build-environment-spec.md`
(checklists and item-level status live in tickets, not in ADRs).
Project overview / build / test quickstart: repo-root `README.md`.

## Roadmap (post-grilling, locked)
M0 → M0.5 → M1 Minimal Renderer → M2 Asset Pipeline → M3 Noto Vertical Slice → M4 Scene →
M5 ImGui Editor → M6 Save/Load. Three parallel tracks: Infrastructure / Asset / Editor.

## Accepted decisions (see ADRs for detail)
- ADR-0001: Asset spike first; official mesh NOT yet published (watch: Unicode 18); fallback cube
  is a permanent **explicit test asset** (NOT an implicit runtime substitution for failed
  loading — never `if (load_failed) useCube()`); asset absence/failure must never block renderer
  development (the M0.5 spike precedes M1).
- ADR-0002: Official 3D PNG = reference_image (metadata-only); placeholder OBJ = geometry driver;
  geometry changes are manifest-level migrations without Scene schema changes (no
  `geometry_source`-swap-only narrowing). Curated 5–8 PNGs across minimal placeholders.
- ADR-0003: Project-owned deps need no sudo/apt/pkg-config/ninja; filesystem contract = external
  libs live in `lib/<name>` (mirrors ../reimagined-meme), fetched via FetchContent explicit
  `SOURCE_DIR` (never `build/_deps`): GLFW 3.4 X11, GLM 1.0.1, ImGui v1.92.9-docking,
  nlohmann/json v3.11.3, tinyobjloader v1.0.6 (drawn in by the M2 spike), spdlog v1.14.1 (Log
  backend per ADR-0008 amendment); glad + stb_image committed under `lib/` (gitignore exceptions).
  Machine prereqs may need sudo once (Q9): `sudo apt install libxinerama-dev libxcursor-dev
  libxi-dev` (verified set, Sep 2026). GLFW is built X11-only (`GLFW_BUILD_WAYLAND=OFF`) so its
  Wayland xkbcommon probe never requires pkg-config. Pinning hardened by 0010: all six Git deps on
  immutable commit SHAs (each resolves to its release tag); no archive sources → `URL_HASH` n/a.
- ADR-0004: OpenGL 3.3 Core / GLSL 330; numeric capability check via GL_MAJOR_VERSION /
  GL_MINOR_VERSION + GL_CONTEXT_PROFILE_MASK; sRGB framebuffer, color/diffuse textures use
  GL_SRGB8_ALPHA8 where applicable; MSAA 4x; debug GL context in Debug builds (verify GL_KHR_debug
  first); swapInterval(1); DPI-safe viewport.
- ADR-0005: depth GL_LESS; cull GL_BACK/CCW + per-material cullEnabled; straight alpha; linear
  clear color (0.12,0.12,0.14); pass ownership (Begin3DPass…/ImGuiPass); no transparent sorting
  (documented limitation).
- ADR-0006: 32 B vertex (pos+normal+uv), GL_UNSIGNED_INT; normals computed + logged; UV `(0,0)` +
  warn; shader never branches on missing data; mesh immutable after GPU upload;
  Model = vector<SubMesh>; Material = baseColor(linear)/opacity/cullEnabled/texture?/shininess(32);
  effectiveAlpha = baseColor.a * opacity; Y-up / -Z forward.
- ADR-0007: manifest.json v1 = single source of truth; stable ASCII id; invariant:
  `geometry != null → geometry_source ∈ {placeholder, official}` and
  `geometry == null → geometry_source == null`; reference_image metadata-only; relative paths,
  no `../`, no runtime network; axis baked at load; dedup by canonical path; SUCCESS/WARN/FAILED;
  FAILED = asset unavailable + diagnostic; scene/entity consumers must not render failed assets, no
  implicit fallback (sole explicit opt-in:
  `fallback_geometry?`, unset for all MVP assets); explicit reload() only. Amendment (0012/0014):
  standalone `loadModelShapesOnly()/loadModelFile()` are an app-facing escape hatch taking an
  explicit caller-trusted absolute path (e.g. NOTO3D_ASSETS_DIR-derived demo fixture, deliberately
  kept out of the manifest); the path predicate still governs every manifest field and every
  `diffuseTextureRef` (0013) on both load paths — never bypassed.
- ADR-0008: C++20; noto3d::* namespaces; **headers are declaration-only — every definition
  lives in a `.cpp`**; src/app|core|renderer|asset|scene|serialization|ui (`src/ui/` created by
  ticket 0028, target `noto3d_ui`: presentation + input adaptation only, links ImGui/GLFW;
  boundary exceptions, no-throw render loop; custom Log facade over spdlog (log_init + NOTO3D_*
  macros, single replaceable call-site spelling; spdlog fetched into lib/spdlog, pinned — ADR-0008
  amendment); -Wall -Wextra -Wpedantic, -Werror optional; CMake presets dev/release;
  dependency-free tests, headless; git on main, .context ignored.
- ADR-0009: M3 vertical slice — scope = existing 6 frozen assets only (no new assets/OBJs/schema,
  no renderer/GL/text capability). Chain: 0015 baseline fence (pure verification; runtime-string
  changes deferred to 0017) → 0016 fixture freeze (manifest contract verified DIRECTLY via JSON +
  filesystem + SHA-256, never through the AssetRegistry; `kM3FrozenAssetIds` = exactly the 6 IDs,
  `demo_checker` excluded) → 0017 observability (single identity line on every selection, dynamic
  window title `<id> [<source>] | noto3d (M3)`, pre-GL `--list-assets` diagnostic CLI) → 0018
  human category verification (one-shot manual matrix; exclusions locked: no runtime reference
  loading, no screenshot/readback, no similarity, no ReferencePreview — M5 UI territory).
  M4 deferred until M3 closes.
- ADR-0010: M4 scene composition (grilling Q1–Q8, locked; hardened pre-implementation by an
  Amendment — camera spherical convention/initial state/signs, `--demo` = demo-only scene,
  `DrawItem` pointer lifetime + callable-Resolver, verification tiers). Pure-data Scene (`EntityId`
  monotonic from 1 / `0`=Invalid / never reused / id≠index; flat world-space Entities;
  `scene::modelMatrix` = radians YXZ T·R·S; `clear()` keeps the counter; M6 = snapshot-replace
  forward contract). Resolution stays out of Scene: `app::collectDrawItems(scene, resolver)` →
  per-entity `DrawItem`s, unresolved → skip (headless 0021 contract), one seam for every CLI path
  (default `m4_vertical_scene` fixture, `--asset` single-entity, `--demo` demo-only Scene with
  exactly one `demo_checker` Entity at identity;
  `--asset`+`--demo` → exit 1; no `--scene=`). `renderer::Camera` = pure-math orbit/zoom/pan
  (locked spherical offset + initial yaw/pitch/distance → exact M3 eye, pitch ±89°, zoom distance
  0.5–20 independent of near/far, pan in world units, no Input abstraction). Close = automated
  matrix (exit codes + structured runtime-log / `entities=<N>` assertions) + manual evidence
  (xprop `WM_NAME` titles, visual smoke); no readback/similarity.
- ADR-0011: M5 editor architecture (grilling Q0–Q14, locked; **execution tickets 0028–0034 written;
    0028 and 0029 implemented, machine-verified, manual evidence pending for both**). Docked layout; one explicit framebuffer-space `ViewportRect` (plain data,
  no ImGui types) as the single source of view extent, driving projection aspect, cursor
  window→viewport-local mapping + containment, and the app-established `glViewport`/scissor before
  the 3D pass (`Begin3DPass()` stays UI-agnostic; ADR-0005 amended; ADR-0004 amended since
  viewport ≠ framebuffer). Editor: **`Editor` owns `Scene` by value** + `EditorState`, exposing
  only `EntityId`-keyed semantic operations — the UI receives the facade and structurally cannot
  obtain a mutable `Scene&`, `Entity*`, or span, so the `EntityId`-only write path is a type
  boundary, not a convention; render path remains `collectDrawItems(editor.scene(), resolver)`
  (single M4 resolution seam, borrowed `DrawItem::model` unchanged). Editor logic (selection,
  edits, hierarchy, asset filtering, viewport geometry) is ImGui-free in `noto3d_app`
  (links neither ImGui nor GLFW) so it stays headless-testable; `src/ui/` is presentation/input
  adaptation only. State lifetimes split three ways: `EditorState` (session, selection),
  `ViewportState` (per view: `Camera` + `ViewportRect`; one view ships, per-view shape),
  `InputState` (per frame). Input ownership is a binary gate — `viewportHovered &&
  !IsAnyItemHovered() && !IsAnyItemActive() && !globalModalCapture`, wheel under the same gate
  with `NoScrollbar|NoScrollWithMouse` on the viewport; **non-obvious constraint: `!
  io.WantCaptureMouse` must never be the viewport camera gate, because the viewport is itself an
  ImGui window and that spelling kills camera input permanently**; camera semantics + cursor
  baselines are 0026, not slice 1. Asset picker takes an app-built value snapshot (never a
  `std::span` into registry storage; renderable-only, hidden counts logged). Observability via
  the existing structured log, committed on `IsItemDeactivatedAfterEdit()` (continuous) / on event
  (discrete); frozen `WM_NAME` title contract and the no-readback/similarity exclusion continue.
  Transform editing is inspector-numeric only: `Scene` keeps radians YXZ, degrees are a
  presentation-layer conversion, fields labelled `Pitch (X)`/`Yaw (Y)`/`Roll (Z)` with the applied
  order Y→X→Z shown (storage letter order ≠ application order), Scene value is the widget's
  per-frame source of truth, displayed precision is authoritative on commit. **Excluded:** undo/redo
  (no command history, no apply-on-release, no destructive destroy/recreate undo), gizmos/direct
  manipulation (would make input ownership three-way), the asset CPU/GPU refactor (0027 = M6 gate;
  M5 adds no GPU-resource ownership — asset editing mutates `assetId` only), M6 save/load, multi-view
  layouts, scene graph/prefab/ECS. Verification = headless `tests/app/` suites for the semantics +
  automated log/exit-code assertions + manual docking/smoke evidence.

## Key external facts (Sep 2026)
- googlefonts/noto-emoji: zero mesh files on any branch; `3D/` = fonts (TTF) + png (≤512px);
  license SIL OFL 1.1.
- Noto 3D OBJ: "coming with Unicode 18" (issue #549, closed).

## Watch items
- [ ] Official Noto 3D OBJ release — when published, update manifest geometry_source → official
      for curated IDs, verify winding/axis against M0.5 contract, re-pin sources.
- [x] Replace GIT_TAG pins with commit SHA (+ URL_HASH) once dev build is stable
      (authored as ticket 0010 — **completed**; all six deps on immutable SHA).
- [x] M3 vertical slice (ADR-0009; 0015–0018).
- [x] M4 scene composition (ADR-0010; 0020–0024) — **complete**, archived.
- [x] 0025 M4 code-cleanup pass — **complete**, archived (independent of M5).
- [ ] M5 ImGui editor — architecture **locked in ADR-0011** (Q0–Q14); execution tickets **0028–0034
        written; 0028 + 0029 implemented and machine-verified (ctest 18/18 dev + release), manual
        evidence pending for both**. 0028 = docked shell + `ViewportRect` + input-ownership gate (frontier
        root; 0026 attaches to it) → 0029 = **done**: Hierarchy + selection + `Editor` facade → 0030 (transform
      editing) and 0031 (asset picker) in parallel → 0032 (create/destroy, needs 0031's asset
      choice) → 0033 (M5 close: matrix + manual docking smoke, blocks on 0030 + 0032).
        **0034** (viewport/input contract hardening: `contains()`'s reserved role + gate-read
        ordering) is blocked by 0028, runs docs-only, and does not block the M5 close.
        **0035** (declaration-only conformance: 10 remaining inline definitions — 8 implicit-inline
        accessors in `renderer`/`ui` headers + 2 free `inline` conversions added by 0030's
        `transform_edit.hpp`; 2 of the 10 are dead and deleted outright) is code-quality, independent
        of the M5 chain, and does **not** block 0030.

## Execution tickets (active ones live in docs/ticket/; completed archived at docs/archive/ticket/)
- M0: 0001 (spec) + 0002–0004 (execution) — complete
- M1: 0005 (Mesh GPU upload), 0006 (Material/shader + first draw), 0007 (pass hygiene,
  superseded by 0012) — complete
- M2: 0008 (AssetRegistry + OBJ/MTL→Mesh), 0014 (demo texture fixture), 0009 (sRGB diffuse
  texture pipeline + E2E), 0013 (texture path safety) — complete
- 0011 (M1 code-quality deferred — ⑦⑧⑨⑩) — folded into 0012, complete
- 0012 (M1 app/renderer separation + registry integration) — complete; closes 0009's E2E gate
- 0010 (dependency pinning) — complete; infra track

### M3 (spec embedded in ADR-0009; complete, archived)
- 0015 (M3 baseline gate), 0016 (asset-set freeze: manifest_integrity_test.cpp), 0017 (selection
  diagnostics + window-title identity + `--list-assets`), 0018 (human Noto-category verification:
  6/6 PASS manual matrix, 3 shared placeholder silhouettes observed) — complete, archived

### M4 (spec embedded in ADR-0010; complete, archived)
- 0020 (Scene foundation: EntityId/Transform/Entity/Scene + modelMatrix) — complete, archived
  (ctest 6/6 dev + release; 28 headless checks; glm::eulerAngleYXZ experimental opt-in noted)
- 0021 (draw-list seam: `app::collectDrawItems`) — complete, archived (7 blocks/15 checks;
  empty Resolver ⇒ `std::logic_error`; borrowed model pointer; suite count 6→7)
- 0022 (multi-entity runtime: CLI→Scene, no legacy render path) — complete, archived
  (default `m4_vertical_scene`=3 / `--asset`=1 / `--demo`=1 demo_checker; `M4_INFO` log; titles;
  invalid flag → exit 1; no `--scene=`)
- 0023 (Camera orbit/zoom/pan + GLFW wiring) — complete, archived (13 blocks/20 checks → ALL
  PASS; suite count 7→8)
- 0024 (integration/regression matrix + manual smoke, M4 close) — complete, archived (CLI matrix
  + `M4_INFO`/`M2_OK` logs green dev+release, ctest 8/8; xprop titles + user-confirmed smoke)

### M4 (post-close cleanup; in docs/ticket/)
- 0025 (M4 code cleanup, narrowed after strict re-review) — **complete, archived**
  (`docs/archive/ticket/0025-m4-code-cleanup.md`), under a top-level invariant that no externally
  observable M4 behaviour or established architecture contract changed (held):
  - ① CLI parsing now lives in `src/app/cli_options.{hpp,cpp}` as `noto3d::app::Options` +
    `parseOptions()`, unit-tested headlessly by `tests/app/cli_options_test.cpp` (12 blocks /
    40 checks on `Options` **content**; CTest `app-cli-parse-contract`, suite 14→15) — the CLI
    contract no longer needs a forked subprocess. Parsing is a pure syntax contract: `--asset` +
    `--demo` exclusion stays pre-GL in `main()`, unknown-asset validation stays in the registry
    layer, and the existing optional/flag shape was kept (no mode enum).
  - ④ `run()`'s bare `{ ... }` block became `int renderScene(GLFWwindow*, const Options&)`; the
    GL-owning objects (shader, registry → Mesh/Texture, renderer, camera) are locals of the helper
    and die with it while the context is current, making that lifetime rule (ticket 0012) structural.
    `run()` keeps only the GLFW lifecycle.
  - ② the 0011⑩ uniform-location cache was verified (hit → no `glGetUniformLocation`; miss → query
    + store; cache cleared whenever the program handle is invalidated — there is no `reload()`) and
    left byte-identical.
  - ③ `loadManifestOnly()` is a public **production** API (backing `--list-assets`, 0017/ADR-0009)
    plus the CPU-only path used by asset tests; removal / `testing`-namespace / `#ifdef` gating
    rejected. The CPU/GPU restructuring question is handed to 0027.
  - ⑤ `noto3d_asset` → `noto3d_renderer` stays **PUBLIC** (0027 owns it); ⑧ the real CMake target
    graph is recorded in the ticket (no overshoot fixed).
  - Verification: red-first TDD, ctest **15/15 dev + release**, 0024 exit-code matrix and
    `xprop` titles unchanged.
- 0026 (M5 camera interaction semantics & cursor baseline) — **active in M5, blocked by the
  first M5 slice's input-ownership gate** (ADR-0011 Q9/Q10 fixed input ownership centrally, so
  this ticket no longer owns it). Scope narrowed to gesture→operation mapping, wheel routing,
  sensitivity config owned by the app/editor input layer — **not** `renderer::Camera` (ADR-0010 Q7
  purity preserved) — and cursor-baseline re-establishment (widget-drag end, focus regain, camera
  drag start/end; deltas denied by the gate are discarded, not deferred)
- 0034 (M5 viewport/input contract hardening) — **ready-for-agent**, blocked by 0028, docs/comment
  only, does not block the M5 close. Records two contracts found by the 0028 review: ① viewport
  input ownership uses ImGui hover + Q10 item arbitration, **not** `ViewportRect::contains()` (so
  Q0's "cursor mapping + containment" wording is amended; `contains()` stays headless-covered,
  framebuffer-space, and reserved for 0026/future picking); ② the four gate inputs must be read
  **before** the 3D panel submits its own items, because `Dummy()` calls `ItemAdd()` and would
  otherwise close the gate permanently — the same silent-death class as the `WantCaptureMouse`
  spelling Q10 forbids
- 0027 (asset CPU/GPU boundary architecture spike) — **M6-preceding gate** (ADR-0011 Q1), not an
  M5 prerequisite; premise corrected: a CPU/GPU boundary **already exists** (no direct GL calls in
  `src/asset/`; `loadObjShapes`/`CanonicalShape`/`loadModelShapesOnly` CPU-only; `uploadToGpu()`
  is the single upload gate) and what remains is header/type coupling (`AssetRegistry` public API
  exposes `renderer::Model*` + caches a `Texture`). M5 adds no GPU-resource ownership; asset
  editing mutates `assetId` only. Must preserve the M4 contracts (AssetRegistry authority, Scene
  pure data, app-layer `collectDrawItems` seam, borrowed `DrawItem::model`)

## Known MVP limitations
- The dev preset's LeakSanitizer reports a fixed 112-byte / 2-allocation leak on any GL run
  (verified identical before and after ticket 0028's refactor). Every stack frame is
  `libLLVM`/`libubsan` — it is Mesa's llvmpipe software-rasteriser JIT under a display-less
  environment, not project code. It does **not** appear on `--list-assets` (no GL context), which
  is the quickest way to confirm the cause.
- No transparent-object depth sorting.
- Changing asset geometry requires manifest edit + explicit registry reload (by design).