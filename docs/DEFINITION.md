# DEFINITION.md — Canonical project vocabulary

This document defines the canonical meaning of project nouns for **emoji_editor**. Code, issues,
and dialogue must use these terms with these meanings. If a term is used elsewhere and disagrees
with this file, this file wins.

Document hierarchy (per `docs/DOCUMENTATION-LIFECYCLE.md`):
- `DEFINITION.md` — canonical meaning of project nouns.
- `CONTEXT.md` — current implementation truth (consolidates Accepted ADRs).
- `docs/adr/*.md` — historical decision records and rationale.

---

## Project

- **emoji_editor** — the C++20/OpenGL desktop application that composes scenes from
  **Noto 3D-style emoji** assets. See project description in `CONTEXT.md`.

## Composition / Scene domain

- **Scene** — the current composition being edited (M4+ = `noto3d::scene::Scene`): a **pure-data**,
  flat collection of **Entities** (stable **EntityId**, `name`, `assetId`, world **Transform**).
  It never references the AssetRegistry, a Model, GPU objects, renderer state, or a logger; an
  unresolved `assetId` never invalidates the Scene (the runtime resolves and skips it).
- **Entity** — one instance of an **asset** in a scene. M4 shape:
  `{ EntityId id; std::string name; std::string assetId; Transform transform; }`. Since M4 is a
  flat world-space list, `transform` IS the world transform (no hierarchy). Per-entity
  Material/MaterialOverride is deferred to M5/M6 (the old "entity = id + transform + material"
  phrasing predates ADR-0010).
- **EntityId** — `std::uint32_t`, `0` = **InvalidEntityId**. Monotonic counter from 1 for the life
  of the Scene; **never reused** (destroyed ids, and `clear()` does not reset the counter). An
  identity, never a storage index (id ≠ vector position). The single identity (runtime =
  persistent) for M6.
- **Transform** — per-entity position / rotation / scale, in Y-up, −Z forward **world space**.
  Rotation = Euler **radians**, YXZ order: `x = pitch`, `y = yaw`, `z = roll`.
- **modelMatrix** — `scene::modelMatrix(transform)`: the single transform→matrix definition,
  `M = T·R·S`, YXZ via `glm::eulerAngleYXZ` (sole implementation; an experimental GTX API —
  `GLM_ENABLE_EXPERIMENTAL` stays TU-local). 0022/0024 never recompose matrices.
- **Scene snapshot (M6 forward contract)** — a Scene JSON file is a **complete snapshot**; loading
  **replaces** the whole Scene (entities + id allocation state), never merges; loader sets
  `nextEntityId = max(restored ids) + 1` (empty → 1).
- **m4_vertical_scene** — the default M4 fixture scene: `smile@(-1.5,0,0)`, `crown@(0,0,0)`,
  `sunglasses@(1.5,0,0)`.
- **demo_checker / demo mode** — `--demo` runs the **0014 demo fixture as a demo-only Scene**
  (never an append to the default scene): the Scene holds **exactly one Entity**
  (`assetId = "demo_checker"`, identity transform). `demo_checker` is an **out-of-manifest**
  fixture entity/asset (geometry_source = placeholder) resolved via the registry's standalone
  `loadModelFile` path (ADR-0007 amendment), carrying a small sRGB checker texture.
- **DrawItem** — frame-local orchestration data `{ const renderer::Model* model; std::string
  assetId; glm::mat4 modelMatrix; }` produced by `app::collectDrawItems`. `model` is a **borrowed
  pointer**, valid only for the current frame / draw-list result — never retained across
  `AssetRegistry` mutation/reload. No cache/dedup (asset identity ≠ entity identity).
- **Resolver** — `app::collectDrawItems`' injectable `(assetId) -> const Model*` seam; null ⇒
  skip, not failure. **Must be callable**: an empty resolver is a programming error, never a
  supported runtime state.
- **Camera** — `noto3d::renderer::Camera`: a pure-math orbit/zoom/pan value object (no GL/Scene/
  GLFW). Spherical convention is locked in ADR-0010 Q7 (eye = target + spherical offset,
  `yaw/pitch/distance`); initial state reproduces the exact M3 eye `(0,0.5,3)`; `orbit` yaw/pitch
  signs, `pan` world-unit camera-local semantics, and `zoom` `distance = clamp(distance - delta,
  0.5, 20)` are contracts, not implementer choices.

## Asset domain

- **Asset** — a curated emoji in the registry, identified by a stable ASCII **id**, carrying
  `reference_image`, `geometry`, `geometry_source`, and metadata (license, sha256, revision).
- **reference_image** — an official Noto 3D PNG render used as visual ground truth and
  **metadata only**; it is never implicitly promoted to a runtime texture. The M0.5 curated set
  uses the 512 px variants where available.
- **geometry** — the geometry resource referenced by an **Asset** and resolved by the
  **AssetRegistry** into a **Model** at load. Null if no geometry is available.
  Resolution chain: `manifest geometry = "…"` → Asset → AssetRegistry → Model → SubMesh → Mesh.
- **geometry_source** — `placeholder` | `official`. Invariant:
  `geometry != null → geometry_source ∈ {placeholder, official}`;
  `geometry == null → geometry_source == null`.
- **placeholder** — locally authored geometry (CCW-outward, Y-up, unit-scale) used until
  official meshes ship. Validated with 0 inverted faces (`assets/tools/gen_placeholders.py`).
- **official mesh** — the Noto 3D OBJ from `googlefonts/noto-emoji`. Status **PENDING**
  (issue #549: "coming with Unicode 18"). Replaces placeholder via **geometry migration**.
- **geometry migration** — a manifest-level change that changes the geometry referenced by an
  Asset without changing the Scene schema. The initial planned migration is placeholder →
  official, followed by axis/winding verification and an explicit reload.
- **Manifest** — `assets/manifest.json` (v1): the authoritative declaration of the asset set
  consumed by the runtime. `assets/tools/gen_manifest.py` may validate or generate boilerplate
  entries, but it must not introduce an independent source of truth.
- **AssetRegistry** — a read-only query interface over the currently loaded Manifest state. Its
  state may be replaced **only** by explicit `reload()`. Load results:
  **SUCCESS** / **LOADED_WITH_WARNINGS** (recovery counters) / **FAILED**. `FAILED` disables the
  asset with a diagnostic; no implicit fallback.
- **fallback cube** — `assets/test/cube.obj`; a permanent **explicit test asset**, **not** a
  runtime substitution mechanism for failed loads. The engine never does
  `if (load_failed) useCube()`. Distinct from the **fallback_geometry** manifest opt-in.
- **fallback_geometry** — an explicit manifest opt-in (`fallback_geometry?`, ADR-0007) that names
  substitute geometry for a specific asset when its primary geometry fails to load. The engine
  never infers it. Not required by any current MVP asset (left unset); it is the *only* explicit
  substitution path and is unrelated to the permanent fallback cube test asset.

## Geometry / mesh domain

- **vertex** — 32 B interleaved `position vec3 + normal vec3 + uv vec2` (no tangents).
- **index** — triangles, always `GL_UNSIGNED_INT`.
- **Mesh** — immutable triangle data; uploaded once to GPU (single interleaved VBO + EBO + VAO).
- **Model** — `vector<SubMesh>`; shared/aliased by multiple assets via canonical path dedup.
  Must remain safe to reuse across entities.
- **SubMesh** — `{ Mesh, Material }`.
- **SubMesh material (default/imported)** — the material carried by a SubMesh from the imported
  model. An Entity applies material properties as a **runtime override**; a shared Model keeps
  its default material untouched. Effective material at render = entity override over the
  SubMesh default.
- **axis_data** — `up_axis` + `recorded_winding`, recorded per asset. `up_axis` is loader input
  and its coordinate correction is baked into the immutable CPU mesh once at load.
  `recorded_winding` is diagnostic metadata only and never changes GL state.
- **canonicalization** — the loader's job: compute missing normals (area-weighted, + counter),
  canonical missing UV `(0,0)` (+ warning). The shader never branches on missing data.

## Material domain

- **Material** — `baseColor vec4 (linear)`, `opacity float`, `cullEnabled bool`,
  `texture?` (single optional diffuse, sRGB), `shininess float` (default 32).
  Excluded: emissive, metallic/roughness, normal mapping, PBR.
- **effectiveAlpha** — `baseColor.a * opacity`.
- **cullEnabled** — per-material backface-cull toggle (default true); escape hatch for winding
  surprises. Recorded winding never flips GL state.

## Renderer / GL domain

- **Renderer baseline** — OpenGL 3.3 Core, GLSL `#version 330 core`.
- **capability check** — numeric verification via `GL_MAJOR_VERSION` / `GL_MINOR_VERSION` +
  `GL_CONTEXT_PROFILE_MASK` (never `glGetString` prefix parsing).
- **pass** — canonical 3D state established once per pass (`Begin3DPass`/`End3DPass`);
  the ImGui overlay owns/restores its own state. States are set per pass, not per draw call.
- **reference (rendering)** — a source image treated as visual ground truth only (see
  **reference_image**); distinct from **texture**.
- **texture (color/diffuse)** — the only texture kind in MVP; sRGB (`GL_SRGB8_ALPHA8` where
  applicable). Non-color textures (masks/lookups) are out of scope and must not assume sRGB.

## Build / dependency domain

- **lib/** — external dependency home (`lib/<name>`, mirrors `../reimagined-meme`); GLFW, GLM,
  ImGui, nlohmann/json, tinyobjloader and spdlog are cloned here at configure time (gitignored),
  each pinned to an immutable commit SHA (ticket 0010); glad + stb_image are committed
  exceptions (not fetchable).
- **machine prerequisites** — OS/toolchain packages needed to *build* fetched sources (e.g.
  `libxinerama-dev`, `libxcursor-dev`); may require one-time sudo. Distinct from project-owned
  dependencies, which require no sudo.

## Terms to avoid (anti-language)

| Don't say | Say | Reason |
|---|---|---|
| "fallback / substitution" when a load fails | FAILED → disable + diagnostic | ADR-0007; cube is a test asset, not a substitution path |
| "texture" for a reference PNG | reference_image | ADR-0002; metadata-only |
| "model" for a placeholders OBJ | placeholder geometry | placeholder is a geometry source, not the scene's model |
| "`--demo` appends to the default scene" | demo fixture = the whole Scene | ADR-0010 Q6; demo-only mode, never default + append |
| `glGetString(GL_VERSION)` prefix/string parsing for capability detection | numeric capability check | ADR-0004 |