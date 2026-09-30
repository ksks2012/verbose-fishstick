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
pass complete** (0020–0025 archived). **M5 ImGui Editor complete (ADR-0011, grilling Q0–Q14;
execution tickets 0028–0034)** — implemented and machine-verified, **M5 closed on the automated
matrix (scripts/m5-close-matrix.sh, dev + release) + xprop title evidence**; the four manual
docking/edit-smoke items in ticket 0033 are recorded as an **open follow-up** requiring a real
window session (there is no such user session in this environment — ticket 0033 stays
in-progress on those four boxes only). **The 0027 asset CPU/GPU architecture gate is closed
(ADR-0012; execution 0036–0039, non-blocking for M6)**, so M6 Save/Load may proceed on a
determined ownership model. M0
(Environment/build),
M0.5 (Asset contract spike), M1 (minimal renderer + pass hygiene), M2 (0008 → 0014 → 0009 → 0013 +
0012/0011/0010) and M3 (0015 → 0016 → 0017 → 0018) are done and verified. Completed tickets are
archived at `docs/archive/ticket/` (all 36, after the 0036–0039 chain closed 2026-09-29). Working tree state:

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
  scene, `--asset`+`--demo` → exit 1, invalid flag → exit 1). **M4 closed with no `--scene=`; M6
  adds it as a third mutually exclusive entry mode (ADR-0013 Q5)** — the M4 statement is superseded
  by ADR-0010's amendment, not deleted, and `--list-assets` keeps its pre-GL short-circuit.
  pure-math orbit/zoom/pan Camera (spherical convention locked; initial eye exactly `(0,0.5,3)`).
  Closed by an automated CLI/`M4_INFO`/CTest matrix (8/8 dev + release) + xprop title evidence +
  user-confirmed manual smoke; no readback/similarity. CLI→Scene render path is exclusively via
  `collectDrawItems()` — no direct `registry.model() → submit()` bypass.
- 0011 folded items done: ⑦ `#error` shader-dir, ⑧ no inline cube / no `optional` ceremony,
  ⑨ validated `--frames=` (non-numeric/negative exits non-zero), ⑩ uniform-location cache.
- 0010 done: all six Git deps pinned by immutable SHA (each resolves to its release tag);
  no archive sources → `URL_HASH` n/a; dev + release configure/build + `ctest` green.

Next on the roadmap: **M6 Save/Load** — architecture **locked in ADR-0013** (grilling Q0–Q11,
2026-09-29; execution tickets not yet written). Its shape: a headless `noto3d::serialization` layer
(lives in `src/serialization/`, reserved and empty since ADR-0008) that projects `Scene` to/from a
single versioned `.scene` JSON document, with **no** new dependency and **no** in-editor file menu,
file picker, dirty state, or Save-vs-Save-As. `--scene=<path>` is a **third mutually exclusive entry
mode** alongside `--asset`/`--demo`; `--save-scene=<path>` is an **output** option that is not part
of that set and writes the entry scene once, before the render loop, atomically (temp file +
`rename`) and overwriting silently. Load reaches the editor through exactly one new by-value
`Editor::replaceScene(Scene)`, so the M5 type boundary (no mutable `Scene&`/`Entity*`) is preserved;
save needs **no** new `Editor` API because `Editor::scene()` already returns `const Scene&`. Load
is **strict on structure / lenient on references**: a malformed or non-v1 document hard-fails with
a diagnostic and a non-zero exit, while an unresolvable `assetId` is kept, warned about, and
skipped at render (the M4 contract). A configure-time arch guard keeps the serializer physically
unable to link `noto3d_asset`/`noto3d_renderer`/`noto3d_core`/glad, which is what makes that split
structural rather than conventional.
**M5 ImGui Editor is complete** (architecture locked in
**ADR-0011**, execution tickets 0028–0034 written and delivered): docked
layout + explicit framebuffer-space `ViewportRect` driving aspect/cursor/`glViewport`/scissor;
app-layer `Editor` **owns the `Scene` by value** and is the only mutation route, so the UI
structurally cannot get a mutable `Scene&` or `Entity*`; ImGui-free editor logic in `noto3d_app`
so it stays headless-testable; input ownership gate = `viewportHovered && !IsAnyItemHovered() &&
!IsAnyItemActive() && !globalModalCapture`, with the wheel under the same gate —
**`!io.WantCaptureMouse` must never be used as the viewport camera gate**, because the viewport is
itself an ImGui window; transform editing is inspector-numeric in degrees while the `Scene` keeps
radians YXZ). Named exclusions: undo/redo, gizmos/direct manipulation, and the asset CPU/GPU
refactor. ADR-0004 and ADR-0005 were amended in the same change; ADR-0010 is unchanged.
M5 closed on the automated matrix (`scripts/m5-close-matrix.sh`, dev + release), its CTest
green 21/21 throughout, and `xprop` titles verified unchanged; the four manual docking/edit-smoke
items remain an open follow-up (real window session required). ADR-0008, ADR-0011 untouched
(no deviation found; the Q5 `noto3d_ui` separate-library point is carried from 0028).
Per DOCUMENTATION-LIFECYCLE, specs live at
the ADR layer: the M3 spec is embedded in **ADR-0009**, the M4 spec in **ADR-0010**, the M5 spec
in **ADR-0011**; the asset CPU/GPU boundary decision is in **ADR-0012** (its execution tickets are
0036–0039, all now complete and archived). `docs/ticket/` holds execution tickets (0026 is
M5-adjacent, still open, waiting only on 0028; 0035 is code-quality and does not block M6; 0033 is
in-progress on its four manual-smoke boxes; **0040 is the ADR-0012 close-out residue — two ①/③
invariants the ADR states but nothing asserts; code-quality, does not block M6**; 0027 is
**complete and archived — decided in ADR-0012**, its implementation 0036–0039 is **complete and
archived — closed 2026-09-29**), plus the **M6 execution chain 0041–0046 (spec embedded in
ADR-0013)**: 0041 `noto3d_serialization` target + proven-fires arch guard · 0042 `Scene` restore
primitive + `Editor::replaceScene` · 0043 serialize/deserialize happy-path round-trip · 0044
strict-structure/lenient-reference load policy · 0045 CLI `--scene=` entry + `--save-scene=` output
+ atomic file I/O · 0046 M6 close (matrix, two-tier evidence, archiving). Blocking chain: 0041 and
0042 are independent (parallel); 0043 needs 0041+0042; 0044 needs 0043; 0045 needs 0043+0044;
0046 needs 0045, plus
0020–0025, 0027 (the ADR-0012 spike record), 0028–0034 and 0036–0039 archived under accepted ADRs.
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
  monotonic from 1 / `0`=Invalid / never reused **within one Scene lifetime** / id≠index;
  flat world-space Entities;
  `scene::modelMatrix` = radians YXZ T·R·S; `clear()` keeps the counter; M6 = snapshot-replace
  forward contract — **amended by ADR-0013 Q1: a restore begins a NEW Scene lifetime, so the
  allocator is re-derived as `max(restored) + 1` and a destroyed high-water id may reappear;
  `Scene::fromEntities` is the only entry that can do this, and it has no counter setter**). Resolution stays out of Scene: `app::collectDrawItems(scene, resolver)` →
  per-entity `DrawItem`s, unresolved → skip (headless 0021 contract), one seam for every CLI path
  (default `m4_vertical_scene` fixture, `--asset` single-entity, `--demo` demo-only Scene with
  exactly one `demo_checker` Entity at identity;
  `--asset`+`--demo` → exit 1; no `--scene=`). `renderer::Camera` = pure-math orbit/zoom/pan
  (locked spherical offset + initial yaw/pitch/distance → exact M3 eye, pitch ±89°, zoom distance
  0.5–20 independent of near/far, pan in world units, no Input abstraction). Close = automated
  matrix (exit codes + structured runtime-log / `entities=<N>` assertions) + manual evidence
  (xprop `WM_NAME` titles, visual smoke); no readback/similarity.
- ADR-0011: M5 editor architecture (grilling Q0–Q14, locked; **execution tickets 0028–0034 written;
    delivered and machine-verified (ctest 21/21 dev + release); M5 closed on the automated matrix
    (0033); the four manual docking/edit smoke items remain an open follow-up**). Docked layout; one explicit framebuffer-space `ViewportRect` (plain data,
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
- ADR-0012: **Asset CPU/GPU boundary** (ticket 0027 spike, closed 2026-09-29; execution tickets
    0036–0039, none blocking M6). Premise correction holds: the **runtime boundary already existed**
    and is enforced — `src/asset/` makes no direct GL call, `loadObjShapes`/`CanonicalShape`/
    `loadModelShapesOnly`/`loadManifestOnly` are CPU-only, `uploadToGpu()` is the single upload
    gate. What was fixed here is **header/type + link coupling**. ① **Type ownership:** `Vertex`
    (32 B, ADR-0006) and `Material` are the *interchange format*, not GPU resources — `Vertex`
    moves to a new GL-free `renderer/vertex.hpp` and `Material` stays GL-free; same namespace, no
    rename/alias. `Mesh`/`SubMesh`/`Model`/`Texture` stay GPU-owned; `Model` is not moved. The CPU
    record's `Material::texture` is **always `nullopt`** (`uploadToGpu()` is its only writer).
    ①b **Lifetime unchanged and now explicit:** `AssetRegistry` stays the **sole authority and sole
    owner** of both the CPU and GPU caches (a facade over two libraries, not two authorities —
    ADR-0007 verbatim); `DrawItem::model` stays the borrowed frame-local `const renderer::Model*`
    (ADR-0010 Q5), invalidated by `reload()`; the 0012 GL-owning-lifetime rule is untouched.
    ② **Real split:** new `noto3d_asset_cpu` (json+glm PUBLIC, tinyobjloader+spdlog PRIVATE; **no
    renderer / no `noto3d_core` / no glad / no libGL** — spdlog is linked *directly* precisely so
    `noto3d_core` cannot drag `lib/glad/include` back on, and `glm` must be linked explicitly or
    the CPU half silently resolves a system glm instead of the pinned `lib/glm`) holds
    the parse/dedup/manifest core **and the `RegistryResult` aggregation** (`aggregateResults()`,
    GL-free, so ③'s CLI path needs no facade); `noto3d_asset` (unchanged name + public contract)
    depends on it and keeps `uploadToGpu()`, the GPU caches and all six `AssetRegistry` entry
    points. The boundary is **machine-enforced**, not conventional: two configure-time arch guards
    (a link guard, and a preprocessor include-closure guard whose `src/renderer/` check is an
    **allowlist** of `{vertex.hpp, material.hpp}` so any new renderer header fails the build until
    classified) — both derive their inputs from the target's own CMake properties, because a
    hand-written list once drifted and hid a live system-glm-vs-pinned-glm ODR bug.
    ③ `loadManifestOnly()`/`loadModelShapesOnly()` are **kept** (0025③) as thin CPU-only paths;
    escape hatches + the 0013 path predicate unchanged; **`tests/asset` + `--list-assets` stop
    linking GL** (the concrete win — **delivered by 0038**: `asset_registry.hpp` is GL-free,
    `AssetRegistry` is split across `asset_registry_cpu.cpp` / `asset_registry.cpp` over one shared
    `loadManifestCpu()` pipeline, the registry test's link line carries no renderer/glad/libGL, and
    the CLI path holds no `AssetRegistry`; `reload()` stays in the facade with its 2 cases in a
    GL-linked target, no assertions dropped). Rejected: single-lib GL-free header subset (kept as tracer 1),
    keep-as-is, renderer-side upload (inverts M4), split CPU/GPU `Material` (duplicates ADR-0006),
    asset-owned `Vertex`/`Material` copies (32 B layout drift). **Supersedes no prior ADR** — 0006,
    0007, 0008, 0009, 0010, 0011 all stand; this is additive.
- ADR-0013: **M6 Save/Load — `.scene` snapshot persistence** (grilling Q0–Q11, locked 2026-09-29;
    execution tickets not yet written; **spec embedded in the ADR** per DOCUMENTATION-LIFECYCLE).
    M6 is a **persistence core + CLI round-trip**, deliberately *not* an in-editor document editor:
    no file menu, no file picker (`lib/` has no file-dialog library and ADR-0003 constrains new
    deps), no dirty state, no Save-vs-Save-As, no new dependency. **Q0** surface · **Q1** allocator
    is **derived** (`nextEntityId = max(restored) + 1`), so a destroyed high-water id *may reappear*
    after a round-trip — accepted deliberately, because a load starts a new Scene lifetime, nothing
    in M6 observes the repeat, and it keeps the file a pure function of its entity set; this also
    **amends the old "EntityId = single identity (runtime = persistent)" wording**, which overstated
    what the file records, and is now pinned by a test rather than left to chance · **Q2** one
    self-contained `.scene` JSON file, assets referenced by `assetId` only (no geometry/path/hash —
    `manifest.json` stays the single source of truth), rotation in **radians YXZ**, and **not**
    recorded: allocator, selection, camera, viewport · **Q3** load is **strict on structure /
    lenient on references**: unreadable/malformed JSON, non-v1 `version`, missing or mistyped
    field, `id == 0`, duplicate `id`, malformed `transform` all hard-fail with a diagnostic and a
    non-zero exit, while an unresolvable `assetId` is kept as pure data, warned, and skipped at
    render (the M4 contract survives into persistence) · **Q4** `version` must be exactly `1`;
    unknown keys at any level are **ignored**, and a future version may only **add** fields · **Q5**
    `--scene=<path>` is a **third mutually exclusive entry mode** (any two → exit 1, pre-GL);
    **supersedes M4's frozen "no `--scene=`"** (ADR-0010 amended, not edited) · **Q6** load injects
    via exactly one new **`Editor::replaceScene(Scene)` by value**, which clears selection and
    carries the derived allocator with it; I/O stays in the app layer so `Editor` remains an
    I/O-free semantic facade, and **save needs no new API** (`Editor::scene()` already returns
    `const Scene&`) · **Q7** `--save-scene=<path>` is an **output option**, orthogonal to the entry
    set, writing once after the entry scene is built and before the render loop · **Q8** saves are
    **atomic** (temp file + `rename`) and **overwrite silently**; a missing parent directory is a
    diagnostic + non-zero exit, never a silent `mkdir` · **Q9** a loaded scene reports its **file
    stem**, so the frozen `M4_INFO`/title shapes are reused unchanged and the milestone tag stays
    frozen at `(M4)` (the M5 precedent) · **Q10** an empty `entities: []` document is valid, but M6
    adds no flag that *creates* an empty scene · **Q11** a new `noto3d_serialization` target linking
    `noto3d_scene` + nlohmann/json only, guarded by a configure-time arch guard proving it links no
    `noto3d_asset`/`noto3d_renderer`/`noto3d_core`/glad — which is what makes Q3's split structural
    rather than conventional, at the cost of a serialization-local `Result`/diagnostics vocabulary
    (ADR-0008's convention, local because the shared one lives in `noto3d_asset_cpu`).
    **Amends** ADR-0010 (the `--scene=` clause) and `DEFINITION.md` (the EntityId identity wording,
    the snapshot-replace rule, and the new document shape); **supersedes no prior decision** — 0006,
    0007, 0008, 0010, 0011, 0012 all stand.

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
- [x] M5 ImGui editor — architecture **locked in ADR-0011** (Q0–Q14); execution tickets **0028–0034
        written and delivered**; **M5 closed on the automated matrix (dev + release) with CTest
        21/21**, xprop titles unchanged; the four manual docking/edit-smoke items are an open
        follow-up in ticket 0033 (real window session required).

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

### M6 (spec embedded in ADR-0013; execution started)
- 0041 (`noto3d_serialization` target + arch guard proven to fire) — **complete 2026-09-30**:
  target links `noto3d_scene` PUBLIC + nlohmann/json PRIVATE; the shared
  `noto3d_assert_target_reaches_no_gl()` gained a `POLICY` keyword (`interchange` default /
  `closed`) so one function serves both boundaries instead of a per-target twin, and guard messages
  now cite the caller's own ADR; the serializer runs `POLICY closed` (no `src/renderer/`,
  `src/asset/`, `src/ui/`). A defect found while proving it: `-M` echoes a relative include
  un-normalized, so the new project-header match silently missed `../asset/x.hpp` — fixed by
  normalizing closure paths, which also closes the same `../renderer/` bypass on the pre-existing
  policy. Five guard violations proven to abort configure; ctest 22/22 dev + release, both close
  matrices ALL PASS, `--graphviz` shows `noto3d_serialization → noto3d_scene` with nlohmann dotted
  (PRIVATE) and 0 GL symbols in the archive.
- 0042 (`Scene` restore primitive + `Editor::replaceScene(Scene)`) — **complete 2026-09-30**:
  `static Scene Scene::fromEntities(std::vector<Entity>)` is the only way to build a Scene
  carrying pre-existing ids (it did not exist before — `nextEntityId_` is private with no setter).
  The allocator is **derived, never accepted**: no setter, no `(entities, nextId)` overload, so
  `max(restored ids) + 1` is a type-level property. Precondition (non-zero, unique ids) is an
  `assert` in debug and undefined in release — a contract marker following `draw_list.cpp:18`,
  **not** a validation path; document validation stays solely in 0044. `Editor::replaceScene`
  takes the Scene **by value** so the entity set and the derived allocator cross the boundary
  together, then `deselect()`s (selection is session state, never persisted). Save needed no new
  API. 7 mutations run as a red-green proof across **both** suites, **0 vacuous**; three findings:
  the ticket's Q1 bullet said "equals the pre-destroy maximum (`max+1`)" but `max+1` of the
  *survivors* is the destroyed value itself, so the parenthetical contradicted the ticket's own
  invariant and ADR-0013 Q1 (the implementation was right, the test was corrected); a
  `!hasSelection()` / `selectedName()` assertion after a swap is **vacuous** whenever the old id
  no longer resolves, so the suite instead pins the case where the restored scene *contains* the
  selected id, which is what a real load can produce; and `decltype(&Editor::replaceScene)` is
  compared against the whole signature `void (Editor::*)(Scene) noexcept`, because a mere
  callability check would also pass for the `Scene&` spelling ADR-0011 forbids.
  Scene suite `check()` 29 → 46, editor suite 62 → 76 plus 2 `static_assert`s; ctest 22/22 dev +
  release, both close matrices ALL PASS. This change also amends the M4 `EntityId` line below,
  which stated "never reused" flatly and so contradicted the pinned Q1 reuse.
- 0043 (v1 round-trip happy path) — **complete 2026-09-30**:
  `serializeScene(const scene::Scene&)` / `deserializeScene(std::string_view)` and the frozen
  `LoadResult { std::optional<scene::Scene> scene; std::vector<std::string> diagnostics; }`
  vocabulary in `src/serialization/scene_io.{hpp,cpp}` — the **serialization-local** Result type
  ADR-0013 Q11 requires (the shared one lives in `noto3d_asset_cpu`, which the guard forbids
  reaching). Document shape is Q2 verbatim: `{"version":1,"entities":[…]}`, one entity per line,
  `entities[]` in creation order, `transform` vectors `[x,y,z]`, rotation in **radians YXZ**, and
  the allocator/selection/camera/viewport absent by construction (no code path could write them).
  `nlohmann::ordered_json` is used over the default `json` so key order is insertion order and the
  output matches the frozen shape rather than `json`'s alphabetical order. Round-trip float
  exactness is nlohmann's shortest-round-trip dump, asserted with **`==`, not a tolerance**, across
  the full range (`1e-45f` denormal, `±FLT_MAX`, `1/3`) — plus `-0.0f` via `std::signbit`, the one
  value `==` cannot pin since `-0.0f == 0.0f`.
  **Duplicate and `id == 0` rejection is implemented HERE, not deferred to 0044**, because 0042's
  `fromEntities` asserts uniqueness as a *debug-only* precondition: a duplicate-id document
  **aborts the process** (verified `SIGABRT`), which would break 0043's own "never throws, never
  partially-populated" contract. The serializer must never hand `fromEntities` data it cannot
  accept. `id` is read as `int64` **before** narrowing, since `EntityId` is `uint32_t` and a
  negative id would otherwise wrap to a huge unsigned value. 0044 still owns the diagnostic taxonomy
  and the unresolvable-reference contract. The version gate emits **three distinct** diagnostics
  (missing / malformed / unsupported) as ADR-0013 Q4 requires. 20 mutations run as a red-green
  proof, **0 vacuous**, five of them caught by *abort* — those five are what prove the no-throw
  contract is load-bearing rather than decorative. Two defects found by the red run itself:
  `readTransform` was handed the entity object instead of the nested `transform` object (7
  assertions failed on documents the serializer had just written), and `!size() == 3` was a
  no-op guard that would have read `a[2]` out of bounds on a length-2 vector. Review found the new
  CMake comment claiming the suite avoids `noto3d_core` while the link line pulls it in (fixed), and
  the version-diagnostic conflation above (fixed). Suite **81 assertions**; ctest **23/23** dev +
  release (22 → 23), clean builds 0 warnings, both close matrices ALL PASS. Headless claim verified
  rather than assumed: no GL/GLFW/X11 on the link line, passes with `DISPLAY` unset, no
  `.scene`/`.json` opened under `strace`.
- 0044 (strict structure / lenient references) — **complete 2026-09-30, uncommitted at the user's
  request**: the read side now **hard-fails every structural defect** (no `Scene`, one diagnostic
  each) while staying **lenient on references** — the serializer never consults the registry (the
  0041 guard makes it impossible), so an unknown or empty `assetId` is retained verbatim for the
  M4 draw-time seam to skip, and an empty `name` is legal. Diagnostics name the defect, the
  `entities[N]` index and the **expected shape** (`must be an object` / `must be an integer` /
  `must be a string` / `exactly 3 components, got 2`), always under the `scene_io: ` prefix. Two
  findings the review surfaced are load-bearing: **`id` needs an upper bound, not just a lower
  one** — `2^32` narrows to `0` on the unchecked `EntityId` cast, which loads silently with the
  reserved `InvalidEntityId` under NDEBUG and **SIGABRTs** inside `fromEntities` in debug
  (verified in both presets) — and the **entity index must be asserted for nested `transform`
  defects**, not merely emitted. 0043's four ad-hoc structural loops were migrated into **one
  `StructuralCase` table**, deliberately: this repo rejects duplicated policy because two copies
  drift (ADR-0012 ③), and a defect in two tables is one the other half forgets. Each row asserts
  no scene, a diagnostic, the defect named, the expectation stated, the index present, and the
  *neighbour* guard's wording **absent** (guard isolation — three guards would otherwise fire
  interchangeably and mask each other). Guard helpers (`requireField`/`requireString`/
  `requireInteger`) collapsed 11 duplicated sites with every diagnostic string unchanged, so the
  mutant count fell 31 → 27 with every survivor **stronger**. Suite **206 assertions**;
  **27/27 mutations, 0 vacuous**; ctest **23/23** dev + release, 0 warnings, both close matrices
  ALL PASS; headless claim re-verified.
- 0045 (CLI `--scene=` + `--save-scene=` + atomic I/O) — **complete 2026-09-30**:
  `--scene=<path>` is the third mutually exclusive entry mode (ADR-0013 Q5), gated **pre-GL** by a
  mode **count** rather than a pairwise chain — with three modes a pairwise chain silently misses
  one pair — and the diagnostic names every offending flag; the registry resolver is now a single
  shared `registryResolver()` shared by all modes, so the policy cannot drift per mode. File I/O
  lives in a new app-layer `src/app/scene_file_io.{hpp,cpp}` (`readSceneFile`,
  `writeSceneFileAtomic`, `saveSceneTo`, `loadSceneFrom`, `sceneNameFromPath`) — the only
  filesystem-touching code and the only non-test consumer of `deserializeScene()`; `Editor` stays
  I/O-free and needed **no new API**, since load uses 0042's `replaceScene(std::move(...))` and save
  reads the existing `scene()`. Q8 atomicity: the temp file is created **in the destination
  directory** (not `/tmp`) because `rename()` is only atomic within one filesystem, and is named
  `"." + stem + ".tmp." + pid + "." + counter` so it is unique per process *and* per attempt; the
  destination is opened only at `rename()`, so any earlier failure leaves the prior document intact;
  overwrite is silent; a missing parent directory is a diagnostic + non-zero exit and **never** a
  silent `mkdir`. Q9: a loaded scene reports its **file stem**, so `M4_INFO`/title shapes are reused
  unchanged and the tag stays frozen at `(M4)`; `M6_INFO scene-loaded`/`scene-saved` are the only
  new lines. ctest **27/27** dev + release (new dependency-free `app-scene-file-io`, 29 assertions, plus three
  new CTest rows `cli-scene-*-exclusive` for the pre-GL exclusion gate), 0 warnings, both close
  matrices ALL PASS, **12/12 mutations caught, 0 vacuous**.
  Three defects the red run found were all in the *verification*, not the production code — the
  most important being a **false-green row**: `--asset`+`--scene` asserted "exit 1" while pointing at
  a **non-existent** file, so it passed because the *load* failed even with the gate deleted, i.e.
  the gate was untested; it now uses a valid document and checks the diagnostic names all three
  flags (verified discriminating: 1 vs 0). A `/code-review` at `41184b3` then found three more: **ADR-0013
  Q3's "the app layer logs a warning" for an unresolvable `assetId` was unmet** (no trigger existed
  before `--scene=`; `buildEntryScene` now warns once per id, keeping Q3's other half — kept, skipped
  at render, load still succeeds); the **entry-mode set was duplicated** between a hand-written sum
  and the message block (one table now drives both); and the new suite's **"no GL" claim was
  conventional** — it links `noto3d_core`, which PUBLIC-exports glad's include dir — so it now
  carries the same `noto3d_assert_target_reaches_no_gl()` guard as the registry/serialization suites,
  proven to fire. Three mutants (temp staged in `/tmp`; `close()` error ignored; **loss of per-attempt
  temp-name uniqueness**) are **recorded as known gaps rather than counted**: uniqueness had a test
  written for it, and the review deleted it because it pinned the mutant's *text* rather than the
  behaviour, so it passed either way. Also: the ticket's "CLI exit matrix (automated, no display)" is
  only true of the **exclusion** rows — a load/save needs the `AssetRegistry`, whose GPU cache
  requires a live context (ADR-0010 Q5 + 0025), so those rows need a display and belong to 0046. One deliberate deviation from the ticket text: the close matrix's existing rows keep
  the same output and exit codes, but a static assertion that required `registry.model(` to appear
  **exactly twice** (a count that exists only while there are two entry modes) now states the
  *intent* — at least one call, every occurrence inside the resolver's `return`.
- 0046 (M6 close) — **`ready-for-agent`**; unblocked by 0045

### M5 (spec embedded in ADR-0011; implemented, machine-verified, tickets archived)
- 0028 (docked shell + `ViewportRect` + input-ownership gate) — complete, archived (headless
  `app-viewport-rect-math` + `app-input-gate-contract`; `noto3d_core` only)
- 0029 (Hierarchy + selection + `Editor` facade) — complete, archived (headless
  `app-editor-facade`; `Editor` owns `Scene` by value, EntityId-only write path)
- 0030 (Inspector numeric transform editing) — complete, archived (`app-transform-edit`; M5_XFRM
  logs incl. dual-unit line)
- 0031 (M5 asset picker) — complete, archived (`app-asset-picker`; value snapshot, offered/hidden
  counts logged, M5_ASSET assigned line)
- 0032 (M5 create/destroy entities) — complete, archived (`app-entity-edit`; `M5_ENTITY` created/
  destroyed lines, silent no-op destroy)
- 0033 (M5 close: integration/regression matrix + manual docking smoke) — automated matrix +
  xprop evidence complete and recorded in the ticket; the four manual docking/edit smoke items are
  an **open follow-up** requiring a real window session (no such session in this environment). The
  other M5-close boxes (docs, archiving, ADR-0011 exclusions re-confirmed) checked; execution
  summary under `docs/summary/0033-m5-close-integration-regression.md`.
- 0034 (M5 viewport/input contract hardening) — **ready-for-agent**, blocked by 0028, docs/comment
  only, does not block the M5 close. Records two contracts found by the 0028 review: ① viewport
  input ownership uses ImGui hover + Q10 item arbitration, **not** `ViewportRect::contains()` (so
  Q0's "cursor mapping + containment" wording is amended; `contains()` stays headless-covered,
  framebuffer-space, and reserved for 0026/future picking); ② the four gate inputs must be read
  **before** the 3D panel submits its own items, because `Dummy()` calls `ItemAdd()` and would
  otherwise close the gate permanently — the same silent-death class as the `WantCaptureMouse`
  spelling Q10 forbids
- 0026 (M5 camera interaction semantics & cursor baseline) — **M5-adjacent follow-up, still open,
  waiting only on 0028** (ADR-0011 Q9/Q10 fixed input ownership centrally, so this ticket no longer
  owns it). Scope narrowed to gesture→operation mapping, wheel routing, sensitivity config owned by
  the app/editor input layer — **not** `renderer::Camera` (ADR-0010 Q7 purity preserved) — and
  cursor-baseline re-establishment (widget-drag end, focus regain, camera drag start/end; deltas
  denied by the gate are discarded, not deferred)
- 0027 (asset CPU/GPU boundary architecture spike) — **complete (2026-09-29)**, decided in
  **ADR-0012**; execution split into **0036–0039** (CPU-first, none blocking M6). Premise
  corrected: a CPU/GPU boundary **already exists** (no direct GL calls in `src/asset/`;
  `loadObjShapes`/`CanonicalShape`/`loadModelShapesOnly`/`loadManifestOnly` CPU-only;
  `uploadToGpu()` is the single upload gate) and what was addressed is header/type + link coupling
  (`AssetRegistry` public API exposes `renderer::Model*` + caches a `Texture`; `noto3d_asset`
  PUBLIC-links `noto3d_renderer` → glad, so CPU-only tests/`--list-assets` transitively linked GL).
  M5 added no GPU-resource ownership; asset editing mutates `assetId` only. Preserved M4 contracts
  (AssetRegistry sole authority, Scene pure data, app-layer `collectDrawItems` seam, borrowed
  `DrawItem::model`) all hold. **Gate closed — M6 may proceed.**
- 0036–0039 (ADR-0012 execution, CPU-first tracer chain) — **`docs/archive/ticket/`**: **0036 complete
  2026-09-29** (`renderer/vertex.hpp` extracted; `obj_loader.hpp`'s include closure is now
  glad-free — 2 glad entries at HEAD, 0 after; ctest 21/21 dev+release, both close matrices ALL
  PASS) · **0037 complete 2026-09-29** (`noto3d_asset_cpu` created: `asset_cpu.{hpp,cpp}` holds
  `CpuObj`/`isUnsafePath`/`parseManifest`/CPU diagnostics/`aggregateResults`/`CpuCache`;
  `AssetRegistry` public API byte-for-byte unchanged as a facade over it; `nm` → 0 `gl*`/glad
  symbols and only `Vertex`+`Material` of `noto3d::renderer`; two configure-time arch guards, both
  proven to fire; ctest 21/21 dev+release, both close matrices ALL PASS) · **0038 complete
  2026-09-29** (`asset_registry.hpp` is GL-free — re-declares `renderer::Model` as
  `vector<SubMesh>` over an incomplete `SubMesh`, plus `Texture`; `AssetRegistry` split across
  `asset_registry_cpu.cpp` (CPU lib: `loadManifestOnly`/`loadModelShapesOnly`/accessors) and
  `asset_registry.cpp` (facade: `load`/`loadModelFile`/`reload`/`uploadToGpu`), with one shared
  `loadManifestCpu()` pipeline so the two modes cannot drift; `noto3d-registry-tests` link line has
  **no `noto3d_renderer`/`glad`/`libGL`** (dev+release), guarded by a third arch guard over the
  test's include closure since `noto3d_core` contributes glad's *include dir*; `--list-assets` now
  uses `parseManifest()`+`loadManifestCpu()` with no `AssetRegistry` and is byte-identical, proven
  with a warning-producing probe entry; `reload()` stays in the facade per ADR-0010, so its 2 cases
  moved to `noto3d-registry-reload-tests` — **no assertion dropped**, ctest **22/22** dev+release
  (ticket invariant amended from 21/21), both close matrices ALL PASS; post-review: GPU dedup probe
  re-ordered back *after* the `fallback_geometry` resolution to match pre-split behaviour, and the
  ordering is now pinned by a red-green test — see `docs/summary/0038-asset-cpu-no-gl-link.md`)
  · **0039 close-out complete 2026-09-29** — the final target graph was read out of CMake itself
  (a `cmake_language(DEFER)` probe, not copied from the ADR) and matches ADR-0012 ② including the
  0037 amendment; the **top-level "zero observable change" invariant holds structurally**: the whole
  0036–0039 chain touched **no** file in `src/scene/`, `src/app/draw_list.*`, `src/app/editor.*` or
  `src/ui/`, and every public declaration of `AssetRegistry` is byte-identical to pre-chain
  (`1a57a3f`; only the private side changed — `doLoad(path, mode)` split into `doLoadFull` +
  `doLoadManifestOnly` over a shared `runManifestPipeline`). Clean dev+release builds 0 warnings, ctest **22/22** in both, both close matrices
  ALL PASS, CLI exit-code matrix unchanged, GL-free link line + 0 GL/glad symbols, and the
  tier-B `xprop` `WM_NAME` titles verified for all three entry modes on a real display.
  Code-quality; **does not block M6**.
- 0035 (declaration-only header conformance) — code-quality, independent of the M5 chain; does
  **not** block M5 close or M6. Ten remaining inline definitions (8 implicit-inline accessors in
  `renderer`/`ui` headers + 2 free `inline` conversions added by 0030's `transform_edit.hpp`; 2 of
  the 10 are dead and deleted outright). Still open in `docs/ticket/`.

## Known MVP limitations
- The dev preset's LeakSanitizer reports a fixed 112-byte / 2-allocation leak on any GL run
  (verified identical before and after ticket 0028's refactor). Every stack frame is
  `libLLVM`/`libubsan` — it is Mesa's llvmpipe software-rasteriser JIT under a display-less
  environment, not project code. It does **not** appear on `--list-assets` (no GL context), which
  is the quickest way to confirm the cause.
- No transparent-object depth sorting.
- Changing asset geometry requires manifest edit + explicit registry reload (by design).