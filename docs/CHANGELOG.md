# CHANGELOG

## [Unreleased] — M4 Scene Composition & Transform (complete; 0025 cleanup pass complete)

### M4 — scene foundation, draw-list seam, multi-entity runtime, camera, close (0020–0024; complete)
- 0020–0024 opened after a grilling session (Q1–Q8) whose decisions are locked in **ADR-0010**
  (M4 spec embedded at the ADR layer per DOCUMENTATION-LIFECYCLE; execution tickets only in
  `docs/ticket/`): pure-data Scene (monotonic never-reused EntityId, flat world-space Entities,
  `scene::modelMatrix` = radians YXZ T·R·S, `clear()`-keeps-counter, M6 snapshot-replace forward
  contract) → app draw-list seam (`app::collectDrawItems`, unresolved → skip) → CLI→Scene entry
  model (default `m4_vertical_scene` fixture, `--asset` single-entity, `--demo` demo-only scene,
  `--asset`+`--demo` exit 1, no `--scene=`) → pure-math `renderer::Camera` (orbit/zoom/pan,
  initial view = M3, pitch ±89°, zoom distance 0.5–20 independent of near/far) → close =
  automated regression matrix + manual smoke (no readback/similarity). CONTEXT updated same change.
- **Contract hardening (pre-implementation, 0021–0024 impacted)**: ADR-0010 Amendment locks the
  camera spherical convention + initial yaw/pitch/distance (exact M3 eye `(0,0.5,3)`) and
  orbit/zoom/pan sign/unit semantics; `--demo` becomes a **demo-only Scene** holding exactly one
  Entity (`demo_checker`, identity transform) — never default + append; `DrawItem::model` lifetime
  (borrowed, frame-valid, fresh each frame) + callable Resolver (empty ⇒ `std::logic_error`);
  verification split into automated (exit codes + structured runtime-log `entities=<N>` assertions
  + CTest) vs manual evidence (`xprop` titles, visual smoke), with unresolved-asset skip proven
  headlessly in 0021. No architecture change; `GLM_ENABLE_EXPERIMENTAL` stays TU-local.
- **M4 delivered and closed (0021–0024)**: execution completed, acceptance checked, 0020–0024
  archived under `docs/archive/ticket/`; ctest now **8/8 on dev + release** (suite count
  5→6→7→8 across scene / draw-list seam / camera). CLI matrix green incl. the new invalid-flag
  row — unknown options (`--bogus`) previously fell through silently to exit 0 and are now
  rejected with exit 1; `--asset`+`--demo` → 1; `--frames=abc`/unknown asset → 1. Structured logs
  verified for all three entry modes (`m4_vertical_scene`=3 / `<id>`=1 / `--demo`=1 `demo_checker`
  + `M2_OK (0009 E2E)`); `xprop` titles verified on X11. Manual smoke (layout + orbit/zoom/pan +
  ESC) user-confirmed; no readback/similarity introduced. CONTEXT updated in the same change.
- **0025 code cleanup complete** (post-M4 pass, narrowed by a strict re-review, archived at
  `docs/archive/ticket/0025-m4-code-cleanup.md`; no public-API removal, no test-only build
  conditionals, no library split, no M5 design pulled into M4):
  - ① `Options`/`parseOptions` extracted from `main.cpp` to `src/app/cli_options.{hpp,cpp}` and now
    unit-tested headlessly by `tests/app/cli_options_test.cpp` (12 blocks / 40 checks asserting
    `Options` **content**, new CTest `app-cli-parse-contract`) — the CLI contract (ADR-0010 Q8) no
    longer needs a forked subprocess. Parsing stays a pure syntax contract: `--asset`+`--demo`
    exclusion remains pre-GL in `main()`, unknown-asset validation stays in the registry layer;
    the existing optional/flag shape was kept instead of introducing a mode enum.
  - ④ `run()`'s ~200-line bare `{ ... }` block extracted into
    `int renderScene(GLFWwindow*, const noto3d::app::Options&)`: GL-owning objects (shader,
    registry → Mesh/Texture, renderer, camera) are locals of the helper and are destroyed while the
    context is still current, so the GL-scope lifetime rule (ticket 0012) is now structural
    instead of
    conventional. `run()` keeps only the GLFW lifecycle. Readability change, zero behaviour change.
  - ② uniform-location cache verified against the three invariants (hit → no
    `glGetUniformLocation`; miss → query + store; cache cleared whenever the program handle is
    invalidated — there is in fact no `reload()` method) and left byte-identical: no gratuitous
    "improvement", no new GL test.
  - ③ `loadManifestOnly()` audited in documentation only: it is the production entry point behind
    `--list-assets` (0017/ADR-0009) plus the CPU-only path used by asset tests; removal, `testing`-namespace
    moves and `#ifdef NOTO3D_TESTING` gating all rejected. Hand-off question recorded in 0027.
  - ⑤ `noto3d_asset` → `noto3d_renderer` stays **PUBLIC** — untouched, owned by 0027.
  - ⑧ actual CMake target graph recorded in the ticket (incl. `noto3d_core` using glad **headers
    only**, `noto3d_scene` → glm only, `noto3d_app` window/GLFW-free with a `renderer::Model*` type
    dependency, headless camera test). No overshoot fixed; three observations recorded instead.
  - Verification: red-first TDD, `ctest` **15/15 on dev + release** (suite 14→15), 0024 matrix
    unchanged (5 valid → 0, 6 invalid → 1), `M0_OK`/`M4_INFO`/`M1_DONE`/`M2_OK` log content and
    order unchanged, `xprop WM_NAME` titles re-verified for all three entry modes.
- 0026 (M5 input & interaction config follow-ups) and 0027 (asset CPU/GPU boundary architecture
  spike) — split out of 0025, deferred; the cleanup ticket stayed free of M5 design under a
  top-level invariant (no externally observable M4 behaviour or established architecture contract
  changes), which held.

## [Unreleased] — M3 Noto vertical slice (complete)

### M3 — asset observability (0015–0018; complete)
- 0015 baseline gate: stale-wording scan across docs (no M2-chain residue); fresh configure +
  build + `ctest` green on dev/dev-clean/release as the M3 baseline. Smoke markers left untouched
  (0017 owns runtime identity).
- 0016 fixture freeze: `tests/asset/manifest_integrity_test.cpp` added — dependency-free headless
  suite that parses `assets/manifest.json` directly (no AssetRegistry) and verifies the M3 frozen
  fixture contract: exactly `smile, warm, cool, crown, top_hat, sunglasses`, `demo_checker`
  excluded, `geometry_source == placeholder` + ADR-0007 nullness invariant, reference/geometry
  files exist and SHA-256 == `metadata.reference_sha256`/`geometry_sha256` (FIPS 180-4 SHA-256
  implemented in-file, KAT-verified; geometry hash doubles as the shared-placeholder invariant),
  `axis_data.up_axis ∈ {x,y,z}`, expected placeholder mapping. ctest now 5/5 on dev + release.
- 0017 selection diagnostics: every selection (default or `--asset=`) prints one identity line
  `M3_INFO selected asset '<id>' geometry=<source> reference=<path>` (old default-only M2_INFO
  line removed); dynamic window title `<id> [placeholder] | noto3d (M3)` via existing
  `glfwSetWindowTitle`; `--list-assets` diagnostic CLI prints `M3_ASSETS` header + one line per
  asset via `loadManifestOnly`/`entries()` and exits 0 before any GL context. No renderer/GL/text
  capability added; frame-limit/asset/demo exit codes unchanged.
- 0018 human category verification: 6-row manual matrix filled by human review — all assets PASS
  (3 shared placeholder silhouettes observed, consistent with the 0016 shared-geometry invariant);
  one informational note on `cool` (reference u1f60e is a smiling face *with sunglasses*; not
  conveyed by base_face — deferred to official-mesh swap). **M3 execution closed**.
- Docs: ADR-0009 records the locked M3 decisions (fence→freeze→observe→verify; fixture contract
  verified independently of `AssetRegistry`; pre-GL `--list-assets`; 0018 exclusions) and embeds
  the M3 milestone spec (§ Spec) — per DOCUMENTATION-LIFECYCLE, **specs live at the ADR layer and
  `docs/ticket/` holds execution tickets only** (0015–0018 roll under ADR-0009); the previous
  standalone spec file (0019) was removed as superseded, its content merged into ADR-0009; CONTEXT
  updated in the same change.

## [Unreleased] — M1 + M2 (complete)

### M2 complete (0008/0014/0009/0013 + 0012/0011/0010)
- 0009 E2E & 0012 registry integration: `noto3d-editor` consumes the `AssetRegistry`;
  `--asset=<id>` renders a manifest asset, `--demo` renders the 0014 fixture with its sRGB
  diffuse texture (`M2_OK ... with diffuse texture (0009 E2E)`). 0011 ⑦⑧⑨⑩ folded in:
  `#error` shader-dir, validated `--frames=` (non-numeric/negative → exit non-zero), inline
  cube/optional ceremony removed, uniform-location cache in `ShaderProgram`. GL-owning objects
  scoped to be destroyed while the context is current.
- 0010 dependency pinning: all six Git deps pinned by immutable commit SHA (each resolves to its
  release tag), `GIT_SHALLOW` retained, no archive sources (`URL_HASH` n/a); fresh dev + release
  configure/build + `ctest` green.

### M2 — asset pipeline (complete)
- 0008 AssetRegistry: manifest.json v1 authoritative load, OBJ/MTL→SubMesh (material grouping,
  up-axis baking, canonical paths, CPU dedup), explicit `fallback_geometry` opt-in, tri-state
  SUCCESS / LOADED_WITH_WARNINGS / FAILED, explicit reload().
- 0009 sRGB diffuse texture: `renderer::Texture::fromFile` (GL_SRGB8_ALPHA8, mipmap/filter
  policy), `uHasTexture` branch (`finalAlpha = effectiveAlpha * sampledAlpha`), registry
  `diffuseTextureRef → Texture` wiring, `GL_SRGB8_ALPHA8` compile-time pin. E2E closed by 0012
  (`noto3d-editor --demo`).
- 0013 texture path safety: shared `isUnsafePath()` predicate reused from manifest paths for
  `diffuseTextureRef`; rejected texture refs promote the asset to `LOADED_WITH_WARNINGS`.
- 0014 demo texture fixture: `assets/geometry/placeholders/demo_checker/` subtree + deterministic
  `assets/tools/generate_demo_texture.py` (stdlib-only PNG writer, byte-identical across runs);
  `map_Kd` relative / no `../`; manifest v1 unchanged.

### M1 — minimal renderer (0005–0007/0012; complete)
- 0005 `renderer::Mesh` GPU upload (interleaved VBO+EBO+VAO, 32B vertex, immutable, RAII).
- 0006 Model/SubMesh/Material types + GLSL 330 base shaders + first visible cube draw;
  `effectiveAlpha = baseColor.a * opacity` with headless test.
- 0007 pass hygiene, delivered as 0012's `renderer::Renderer` (Begin3DPass/End3DPass/submit;
  states once per pass, linear clear, cullEnabled per draw). 0011 deferred findings folded into
  0012 and completed there (⑦⑧⑨ in main.cpp; ⑩ uniform-location caching).

### M0 / M0.5 (complete)
- M0: build & environment baseline (tickets 0001–0004); green presets; M0_OK/M0_FAIL markers.
- M0.5: asset contract spike — manifest v1, placeholder OBJs, 6 pinned Noto reference PNGs.

### M0 — build & environment (execution complete)
- Execution tickets 0002–0004 closed (Q9 prerequisites / dependency build / M0_OK verification);
  acceptance matrix in `docs/ticket/0001` fully checked; ctest green on dev, dev-clean, release.
- `noto3d::core::Log` introduced; DPI-safe viewport with framebuffer-size resize callback
  (ADR-0004); debug-GL gated on `GL_KHR_debug` verification before use.
- Non-interactive verification proven: `--frames=5` → `M0_OK … 3.3+ core profile` +
  `M0_DONE rendered 5 frames`, exit 0; failure path prints `M0_FAIL <reason>` and exits non-zero
  (verified with DISPLAY unset). sRGB + MSAA 4 + depth 24 hints validated on the created context.
- `supportsRequiredContext(major, minor, profileMask)` extracted as a dependency-free pure
  function (`src/core/gl_capability.{hpp,cpp}`); 4-case matrix CTest under `tests/` per ADR-0008
  (3.3 / 4.6 core → ok; 3.2 core / 3.3 compatibility → fail).
- `lib/glad` + `lib/stb` committed (non-fetchable exceptions, ADR-0003); all fetched clones stay
  gitignored.
- Q9 machine prerequisite set finalized: `sudo apt install libxinerama-dev libxcursor-dev libxi-dev`;
  GLFW built X11-only (`GLFW_BUILD_WAYLAND=OFF`) so its Wayland xkbcommon probe never requires
  pkg-config. Clean configure + full dependency build now green on all presets.

### M0 — initial wiring (historical)
- Dependency layout mirrors ../reimagined-meme: external libs live in `lib/` — GLFW 3.4,
  GLM 1.0.1, ImGui v1.92.9-docking, nlohmann/json v3.11.3 cloned there via FetchContent explicit
  `SOURCE_DIR` (gitignored); glad (GL 3.3 Core) + stb_image committed as non-fetchable exceptions.
- CMakePresets: dev (Debug+sanitizers option+debug GL) / release.
- Bootstrap main: GLFW window + 3.3 Core context + numeric capability check + clear.
- Configure reaches GLFW X11 probe; blocked on machine prereq
  `sudo apt install libxinerama-dev libxcursor-dev` (Q9). Build verification pending.

### Documentation (review corrections, round 1)
- ADR-0003 retitled (drop "no sudo"); contract split: project deps need no sudo /
  machine prereqs may need sudo once; FetchContent `lib/<name>` filesystem contract made explicit;
  M0 dependency-verification acceptance checklist added.
- ADR-0001: fallback cube clarified as explicit test asset, not implicit runtime fallback.
- ADR-0004: numeric capability check specified (GL_MAJOR/MINOR + PROFILE_MASK); sRGB limited to
  color/diffuse textures; debug-context requires GL_KHR_debug verification.
- CONTEXT.md/ADR-0008/docs lifecycle: document hierarchy clarified (CONTEXT = current truth,
  ADRs = history; update both in same change).
- New docs/DEFINITION.md: canonical project vocabulary + anti-language table.

### Documentation (review corrections, round 2)
- ADRs carry no checklists: ADR-0003 verification section replaced with a pointer to the M0
  ticket; acceptance matrix (resolves `[x]` / builds `[ ]`, verified vs pending) lives in
  `docs/ticket/0001-build-environment-spec.md` per the to-spec format.
- DEFINITION.md: Manifest = authoritative runtime declaration (gen_manifest.py is a
  validator/boilerplate tool, not a second source of truth); Model/SubMesh material ownership
  (default/imported vs entity runtime override); AssetRegistry described as read-only query
  interface replaced only by explicit reload(); geometry migration reworded (manifest-level
  change, initial path placeholder → official); reference_image decoupled from 512 px
  (curated set prefers it where available); anti-language row corrected to
  `glGetString(GL_VERSION)` prefix parsing vs numeric capability check.
- Uncommitted work is not reported as completed in status docs.

### Documentation (review corrections, round 3)
- DEFINITION.md: geometry defined as geometry resource resolved by the AssetRegistry into a
  Model; geometry migration keeps its general meaning (initial path = placeholder → official);
  axis_data split (up_axis → baked conversion; recorded_winding → diagnostics only);
  fallback_geometry added (explicit opt-in, unset in MVP).
- CONTEXT.md: "assets must never block the renderer" → "asset absence/failure must never block
  renderer development"; CHANGELOG pinned to docs/CHANGELOG.md.
- M0 ticket: "headless" → non-interactive bounded loop (requires a display server; not offscreen
  EGL/headless); capability validation specified as a dependency-free pure function
  (supportsRequiredContext) with 3.3/4.6-core vs 3.2-core/3.3-compat matrix; "baseline renderer
  state" → "required baseline GL state" (renderer starts at M1).
- DOCUMENTATION-LIFECYCLE.md: single CHANGELOG home (docs/CHANGELOG.md); root copy removed.

### M0.5
- 6 pinned Noto 3D reference PNGs (512px, SIL OFL 1.1) under assets/references/noto3d/.
- Placeholder OBJs (base_face/accessory/generic; CCW-outward, Y-up) + cube.obj test asset.
- assets/manifest.json v1 + assets/README.md Asset Contract.
- Asset facts recorded: official mesh PENDING (Unicode 18).