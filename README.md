# noto3d — Noto 3D Emoji Editor (MVP)

C++/OpenGL editor MVP for Noto 3D-style emoji composition. With M0–M5 complete this is a
docked editor built on a real Scene engine: an asset pipeline plus a minimal windowed renderer
(manifest assets the user picks, plus a textured demo fixture), a pure-data Scene model with
multi-entity composition + transforms + camera (M4), and a full ImGui editing surface — select,
transform, swap asset, create, delete — with end-to-end structured observability (M5).

- **Language / API:** C++20, OpenGL **3.3 Core** / GLSL 330, GLFW 3.4 (X11), GLM,
  **ImGui (docking)**, nlohmann/json, tinyobjloader (OBJ/MTL), spdlog (Log backend),
  glad + stb_image (committed under `lib/`)
- **Docs hierarchy:** `docs/CONTEXT.md` = current implementation truth; `docs/adr/` = decision
  history (+ embedded milestone specs); `docs/DEFINITION.md` = project vocabulary;
  `docs/CHANGELOG.md` = milestone history;   `docs/archive/ticket/` = all execution tickets
  (complete, incl. M5 0028–0034 and the 0027 asset-boundary spike); `docs/ticket/` = pending
  (0026 M5-adjacent camera interaction; 0035 header conformance; 0036–0039 the ADR-0012
  `noto3d_asset_cpu` split).

## Status

| Milestone | Scope | State |
|---|---|---|
| **M0** | Build/env baseline (0001–0004) | ✅ complete — green configure/build/verify on dev, dev-clean, release |
| **M0.5** | Asset contract spike (manifest + placeholders + references) | ✅ complete |
| **M1** | Minimal renderer (0005–0007) | ✅ complete — Mesh upload, Material/shader/first draw, pass hygiene |
| **M2** | Asset pipeline (0008–0014) | ✅ complete — `AssetRegistry` + OBJ/MTL→Mesh, sRGB diffuse texture pipeline + E2E demo render, demo fixture, texture path safety |
| **M3** | Noto vertical slice (0015–0018) | ✅ complete — fixture frozen (manifest-integrity contract test, ctest 5/5), selection diagnostics (`M3_INFO` identity line, dynamic title, `--list-assets`), human category verification |
| **M4** | Scene composition & transform (0020–0024) | ✅ complete — pure-data Scene + draw-list seam + multi-entity runtime + orbit/zoom/pan camera; automated matrix + xprop titles + smoke; 0025 code-cleanup pass complete (ctest 15/15) |
| **M5** | ImGui Editor (0028–0034) | ✅ complete — docked shell + `ViewportRect` + input-ownership gate, Hierarchy/selection/`Editor` facade, inspector numeric transform editing (degrees → scene radians YXZ), asset picker (value snapshot), create/destroy; closed on the automated close matrix (ctest 21/21, `scripts/m5-close-matrix.sh`) + xprop titles. Manual docking/edit smoke open as a follow-up (real window session required) |

### Completed tickets (`docs/archive/ticket/`)

- **M0:** 0001 spec, 0002 Q9 machine prereqs, 0003 dependency build, 0004 M0_OK verification
- **M1:** 0005 Mesh GPU upload (`renderer::Mesh`, 32B vertex, immutable), 0006
  Model/SubMesh/Material + GLSL 330 base shaders + first cube draw, 0007 pass hygiene
  (superseded by 0012; shipped as `renderer::Renderer`)
- **M2:** 0008 AssetRegistry (manifest v1, OBJ/MTL→SubMesh, dedup, fallback_geometry, tri-state
  SUCCESS/LOADED_WITH_WARNINGS/FAILED), 0014 demo texture fixture (deterministic
  `generate_demo_texture.py`, `demo_checker` subtree), 0009 sRGB diffuse texture pipeline
  (E2E demo render via `--demo`), 0013 texture path safety (shared `isUnsafePath()`
  predicate), 0012 app/renderer separation + registry integration, 0010 dependency pinning
- **0011** M1 code-quality deferred findings (⑦⑧⑨⑩) — folded into 0012

M4 tickets (0020–0025) and M5 tickets (0028–0034) are complete and archived. **M5's architecture
was locked in ADR-0011** (docked editor layout, `ViewportRect`, `Editor`-owns-`Scene` facade,
input-ownership gate incl. the wheel) and its execution tickets shipped as **0028–0034**; the M5
close is delivered by **`scripts/m5-close-matrix.sh`** (CLI exit-code + structured-log matrix for
every entry mode, M5-op log assertions via the headless app suites, static seam checks) with
**ctest 21/21 dev + release** and `xprop` titles re-verified unchanged from M4. The four manual
docking/edit-smoke boxes of the close remain an open follow-up (require a real window session).
**0026** (M5 camera interaction semantics & cursor baseline, M5-adjacent, waiting on 0028) and
**0035** (header-conformance, code-quality) are the pending side-tickets; the **0027** asset CPU/GPU
boundary spike is **complete (ADR-0012)** with its implementation chain **0036–0039** pending. All
completed execution history lives under `docs/archive/ticket/`. Current milestone: **M5 ImGui Editor complete — M6 Save/Load is
next**.

## Build & test

Machine prereq (one-time): `sudo apt install libxinerama-dev libxcursor-dev libxi-dev`

```bash
cmake --preset dev                      # configure (Debug + sanitizer option)
cmake --build build/dev -j               # build
ctest --test-dir build/dev               # headless tests (21 suites)
build/dev/noto3d-editor --frames=5       # non-interactive smoke: prints M0_OK / M1_DONE
build/dev/noto3d-editor                  # interactive window
```

Structured-log prefixes: `M0_OK` (context/capability), `M1_OK` / `M1_DONE` (renderer, frame
completion), `M2_*` (mesh/texture), `M3_INFO` (asset identity), `M4_INFO` (scene about to render
— logs `entities=<N>`), and the M5 editing ops asserted by the headless app suites: `M5_SEL`
(entity selection, `entity='<name>' id=<n>`), `M5_ASSET` (asset picker offered/hidden counts +
assignment), `M5_XFRM` (transform commits, rotation in degrees+radians), `M5_ENTITY`
(create/destroy, `count=<N>`).

To re-run the M5 close matrix on any build dir (dev axis asserts log content; add `--release` for
exact GL exit codes): `scripts/m5-close-matrix.sh build/release --release`.

Presets: `dev` (Debug + sanitizer/debug-GL options), `dev-clean`, `release`. Our own code builds
with `-Wall -Wextra -Wpedantic`; `-Werror` is opt-in (ADR-0008).

## Layout

```text
assets/                 manifest.json (v1, authoritative) + references/ (official PNGs),
                        geometry/ (placeholder OBJs incl. demo_checker fixture)
src/                    app/ editor facade + entity/transform/picker edit logic + close matrix ·
                        ui/ ImGui presentation & input adaptation (docked editor shell) ·
                        scene/ pure-data Scene (EntityId/Transform/Entity) · core/ Log + GL
                        capability · renderer/ Mesh/Model/Material/ShaderProgram/Texture/Renderer ·
                        asset/ AssetRegistry+OBJ/MTL loader · serialization/ (M6)
shaders/                GLSL 330 base.vert / base.frag
lib/                    pinned deps (glad + stb_image committed; six Git deps fetched:
                        GLFW/GLM/ImGui/json/tinyobjloader/spdlog, each on an immutable SHA)
tests/                  dependency-free headless contract tests (asset/renderer/core/app)
docs/                   CONTEXT/ADR/DEFINITION/CHANGELOG/ticket + archive
```

## Renderer key facts (ADR-0005/0006)

- 3D pass state set **once per frame** by `Renderer::Begin3DPass`; `Material::cullEnabled` is the
  only per-draw variance. The editor shell (M5) confines the 3D clear/scissor to the viewport
  rect and overlays ImGui afterwards — `Begin3DPass()` stays UI-agnostic.
- 32B vertex `pos+normal+uv`, `GL_UNSIGNED_INT` indices, immutable `Mesh` after upload.
- Straight alpha; linear clear `(0.12, 0.12, 0.14)`; sRGB framebuffer converts on output;
  diffuse textures allocate `GL_SRGB8_ALPHA8` (hardware sRGB→linear on sample); shader stays
  linear and never branches on missing vertex data.
- `finalAlpha = effectiveAlpha * sampledAlpha`; `effectiveAlpha = baseColor.a * opacity`.
- App layer (incl. `noto3d_app` editor logic) links **neither ImGui nor GLFW** — editor semantics
  stay headless-testable; `src/ui/` is presentation/input adaptation only (ADR-0011 Q5).

## Asset & texture pipeline (ADR-0002/0007)

`manifest.geometry → OBJ → mtllib → MTL → map_Kd → diffuseTextureRef → Texture::fromFile()`.
Reference PNGs (`assets/references/**`) are metadata-only and never uploaded. Manifest paths and
MTL `map_Kd` share the same path-safety predicate: relative, no `../`, no absolute; a rejected
texture ref promotes the asset to `LOADED_WITH_WARNINGS` (geometry still renders with baseColor).

## Next steps

1. **Manual M5 smoke (open follow-up)** — the four platform-specific boxes of ticket 0033
   (docking move/resize with correct non-square viewport aspect, the full user edit pass,
   interaction-ownership checks, no-gizmo/no-undo confirmation). Everything else about M5 is
   machine-closed; these need a real windowed session.
2. **Pending ticket work**: **0036–0039** (the ADR-0012 `noto3d_asset_cpu` split — extract a
   GL-free `renderer/vertex.hpp`, create the `noto3d_asset_cpu` target, point `tests/asset` +
   `--list-assets` at it so they link no GL, then close out; code-quality, **does not block M6**),
   **0035** (declaration-only header conformance: 10 remaining inline
   definitions in `renderer`/`ui` headers — code-quality, off-chain), **0026** (M5 camera
   interaction semantics & cursor baseline; M5-adjacent, now waiting only on 0028).
3. **M6 Save/Load** — Scene JSON as a complete snapshot (forward contract in ADR-0010 Q4), with the
   previous milestone's decisions carried forward. The 0027 asset-boundary gate is **closed**
   (ADR-0012), so the ownership model this milestone depends on is determined.