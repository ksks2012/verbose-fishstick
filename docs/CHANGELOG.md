# CHANGELOG

## [Unreleased] — M0 / M0.5 (complete); M1 not started
- Grilling session Q1–Q8 closed; foundation decision tree complete (ADR-0001..0008).
- Repository initialized on `main`; Q1–Q8 recorded as ADRs; CONTEXT.md established.

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