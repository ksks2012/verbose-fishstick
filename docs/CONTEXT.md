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
M4 Scene Composition & Transform **complete** (0020–0024, archived; 0025 code-cleanup pass
`ready-for-agent` and 0026/0027 follow-ups deferred — all independent of M5). M0 (Environment/build),
M0.5 (Asset contract spike), M1 (minimal renderer + pass hygiene), M2 (0008 → 0014 → 0009 → 0013 +
0012/0011/0010) and M3 (0015 → 0016 → 0017 → 0018) are done and verified. Completed tickets are
archived at `docs/archive/ticket/` (all 24). Working tree state:

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

Next on the roadmap: **M4 complete → M5 ImGui** (ADR-0005 ImGui-limitation note at the
Renderer's `End3DPass()`; `lib/imgui` already fetched). Per DOCUMENTATION-LIFECYCLE, specs live at
the ADR layer: the M3 spec is embedded in **ADR-0009**, the M4 spec in **ADR-0010**; `docs/ticket/`
holds execution tickets only (currently 0025 code-cleanup, rolled under no ADR), plus completed
0020–0024 archived under accepted ADRs.
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
- ADR-0008: C++20; noto3d::* namespaces; src/app|core|renderer|asset|scene|serialization|ui;
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
- [x] M4 scene composition (ADR-0010; 0020–0024) — **complete**, archived; open 0025 code-cleanup
      pass independent of M5. Next: M5 ImGui editor.

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
- 0025 (M4 code cleanup, narrowed after strict re-review) — `ready-for-agent`, no blocker,
  independent of M5, under a top-level invariant that no externally observable M4 behaviour or
  established architecture contract may change: ① extract CLI parsing → `src/app/cli_options` + headless `Options`-content
  tests (P1), ④ flatten `run()`'s GL scope into `renderScene()` (P2), ② verify the existing
  uniform-location cache (verification-only), ③ audit the `loadManifestOnly()` seam without
  removing/`#ifdef`-gating it (it backs production `--list-assets`, 0017), ⑤ record the
  asset→renderer PUBLIC GL coupling as deferred, ⑧ record the actual CMake target graph. Explicitly
  does **not** remove public APIs, add test-only build conditionals, split libraries, or pre-design
  M5 features
- 0026 (M5 input & interaction config follow-ups) — deferred to M5: cursor-baseline reset semantics
  (firstMove is cursor-ownership, not scene lifecycle) + camera interaction sensitivity config
  owned by the app/input layer — **not** `renderer::Camera` (ADR-0010 Q7 purity preserved)
- 0027 (asset CPU/GPU boundary architecture spike) — deferred; output = investigation → proposed
  boundary → ADR/amendment → implementation tickets. Covers type ownership, CPU/GPU resource
  ownership and lifetimes, library shape, `loadManifestOnly()`/`loadModelFile()` consequences —
  and must preserve the M4 contracts (AssetRegistry authority, Scene pure data, app-layer
  `collectDrawItems` seam, borrowed `DrawItem::model`)

## Known MVP limitations
- No transparent-object depth sorting.
- Changing asset geometry requires manifest edit + explicit registry reload (by design).