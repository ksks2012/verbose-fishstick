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
M2 Asset Pipeline in progress. M0 (Environment/build), M0.5 (Asset contract spike), and M1
(minimal renderer + pass hygiene) are complete; M2 chain 0008 → 0014 → 0009 → 0013 implemented
and verified (0008/0013/0014 completed; 0009 core committed, remains in-progress). Completed
tickets are archived at `docs/archive/ticket/`. Remaining active work:
- **0012 (in-progress)**: `renderer::Renderer` committed (`21a9579`); finish the main.cpp
  slim-down (0011 ⑦⑧⑨ + inline cube fixture) and wire `AssetRegistry` into the app — this also
  closes 0009's manual E2E demo gate.
- **0009 (in-progress)**: manual E2E demo render of the 0014 fixture — pending 0012's registry
  integration.
- **0010 (ready-for-agent)**: pin deps to commit SHA + URL_HASH (ADR-0003 watch item).
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
  nlohmann/json v3.11.3; glad + stb_image committed under `lib/` (gitignore exceptions).
  Machine prereqs may need sudo once (Q9): `sudo apt install libxinerama-dev libxcursor-dev
  libxi-dev` (verified set, Sep 2026). GLFW is built X11-only (`GLFW_BUILD_WAYLAND=OFF`) so its
  Wayland xkbcommon probe never requires pkg-config. tinyobjloader deferred to M2. Pinning: tags
  now, commit-SHA/URL_HASH later.
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
  `fallback_geometry?`, unset for all MVP assets); explicit reload() only.
- ADR-0008: C++20; noto3d::* namespaces; src/app|core|renderer|asset|scene|serialization|ui;
  boundary exceptions, no-throw render loop; custom Log; -Wall -Wextra -Wpedantic, -Werror optional;
  CMake presets dev/release; dependency-free tests, headless; git on main, .context ignored.

## Key external facts (Sep 2026)
- googlefonts/noto-emoji: zero mesh files on any branch; `3D/` = fonts (TTF) + png (≤512px);
  license SIL OFL 1.1.
- Noto 3D OBJ: "coming with Unicode 18" (issue #549, closed).

## Watch items
- [ ] Official Noto 3D OBJ release — when published, update manifest geometry_source → official
      for curated IDs, verify winding/axis against M0.5 contract, re-pin sources.
- [ ] Replace GIT_TAG pins with commit SHA (+ URL_HASH) once dev build is stable
      (authored as ticket 0010).

## Post-M0 execution tickets (docs/ticket/ — active; completed/superseded moved to docs/archive/ticket/)
- 0012 M1 app/renderer separation + registry integration (in-progress): supersedes 0007, folds in
  0011 (⑦⑧⑨ + ⑩). `renderer::Renderer` (Begin3DPass/End3DPass/submit) committed `21a9579`;
  remaining = main.cpp slim-down + AssetRegistry wiring (closes 0009 E2E)
- 0009 M2 sRGB diffuse texture (in-progress): GL_SRGB8_ALPHA8; finalAlpha = effectiveAlpha *
  sampledAlpha; uHasTexture branch; core committed `6225dac`; manual E2E demo render pending
  0012's registry integration
- 0010 dependency pinning hardening (ready-for-agent): ADR-0003 watch item — GIT_TAG → commit
  SHA + URL_HASH; parallel infra track

### Archived tickets (docs/archive/ticket/)
- M0: 0001 (spec) + 0002–0004 (execution) — complete
- M1: 0005 (Mesh GPU upload), 0006 (Model/Material/shader + first draw), 0007 (pass hygiene,
  superseded by 0012) — complete
- M2: 0008 (AssetRegistry + OBJ/MTL→Mesh), 0013 (texture path safety), 0014 (demo texture
  fixture) — complete
- 0011 (M1 code-quality deferred) — folded into 0012 (not executed independently)

## Known MVP limitations
- No transparent-object depth sorting.
- Changing asset geometry requires manifest edit + explicit registry reload (by design).