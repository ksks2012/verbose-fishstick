# noto3d — Noto 3D Emoji Editor (MVP)

C++/OpenGL editor MVP for Noto 3D-style emoji composition. With M0–M3 complete this is an
engine slice — an asset pipeline plus a minimal windowed renderer (manifest assets the user
picks, plus a textured demo fixture) — gaining its Scene model in M4 (multi-entity composition
+ transforms + camera), then interactivity in M5.

- **Language / API:** C++20, OpenGL **3.3 Core** / GLSL 330, GLFW 3.4 (X11), GLM, ImGui (M5),
  nlohmann/json, tinyobjloader (OBJ/MTL), spdlog (Log backend), glad + stb_image (committed under
  `lib/`)
- **Docs hierarchy:** `docs/CONTEXT.md` = current implementation truth; `docs/adr/` = decision
  history (+ embedded milestone specs); `docs/DEFINITION.md` = project vocabulary;
  `docs/CHANGELOG.md` = milestone history; `docs/archive/ticket/` = all execution tickets
  (complete); `docs/ticket/` = pending (0026 M5 camera interaction; 0027 M6-preceding gate).

## Status

| Milestone | Scope | State |
|---|---|---|
| **M0** | Build/env baseline (0001–0004) | ✅ complete — green configure/build/verify on dev, dev-clean, release |
| **M0.5** | Asset contract spike (manifest + placeholders + references) | ✅ complete |
| **M1** | Minimal renderer (0005–0007) | ✅ complete — Mesh upload, Material/shader/first draw, pass hygiene |
| **M2** | Asset pipeline (0008–0014) | ✅ complete — `AssetRegistry` + OBJ/MTL→Mesh, sRGB diffuse texture pipeline + E2E demo render, demo fixture, texture path safety |
| **M3** | Noto vertical slice (0015–0018) | ✅ complete — fixture frozen (manifest-integrity contract test, ctest 5/5), selection diagnostics (`M3_INFO` identity line, dynamic title, `--list-assets`), human category verification |
| **M4** | Scene composition & transform (0020–0024) | ✅ complete — pure-data Scene + draw-list seam + multi-entity runtime + orbit/zoom/pan camera; automated matrix + xprop titles + smoke; 0025 code-cleanup pass complete (ctest 15/15) |

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

M4 tickets (0020–0025) are complete and archived. **M5's architecture is locked in ADR-0011**
(docked editor layout, `ViewportRect`, `Editor`-owns-`Scene` facade, input-ownership gate); M5
execution tickets are not written yet, so the only pending tickets are **0026** (M5 camera
interaction semantics & cursor baseline, blocked by the first slice's input gate) and **0027**
(M6-preceding asset CPU/GPU boundary gate). All completed execution history lives under
`docs/archive/ticket/`. Next roadmap milestone:
**M5 ImGui Editor**.

## Build & test

Machine prereq (one-time): `sudo apt install libxinerama-dev libxcursor-dev libxi-dev`

```bash
cmake --preset dev                      # configure (Debug + sanitizer option)
cmake --build build/dev -j               # build
ctest --test-dir build/dev               # headless tests (5 suites)
build/dev/noto3d-editor --frames=5       # non-interactive smoke: prints M0_OK / M1_DONE
build/dev/noto3d-editor                  # interactive window
```

Presets: `dev` (Debug + sanitizer/debug-GL options), `dev-clean`, `release`. Our own code builds
with `-Wall -Wextra -Wpedantic`; `-Werror` is opt-in (ADR-0008).

## Layout

```text
assets/                 manifest.json (v1, authoritative) + references/ (official PNGs),
                        geometry/ (placeholder OBJs incl. demo_checker fixture)
src/                    app/ main.cpp · core/ Log + GL capability · renderer/ Mesh/Model/
                        Material/ShaderProgram/Texture/Renderer · asset/ AssetRegistry+OBJ/MTL loader
shaders/                GLSL 330 base.vert / base.frag
lib/                    pinned deps (glad + stb_image committed; six Git deps fetched:
                        GLFW/GLM/ImGui/json/tinyobjloader/spdlog, each on an immutable SHA)
tests/                  dependency-free headless contract tests (asset/renderer/core)
docs/                   CONTEXT/ADR/DEFINITION/CHANGELOG/ticket + archive
```

## Renderer key facts (ADR-0005/0006)

- 3D pass state set **once per frame** by `Renderer::Begin3DPass`; `Material::cullEnabled` is the
  only per-draw variance; no snapshot/restore until M5 ImGui overlay.
- 32B vertex `pos+normal+uv`, `GL_UNSIGNED_INT` indices, immutable `Mesh` after upload.
- Straight alpha; linear clear `(0.12, 0.12, 0.14)`; sRGB framebuffer converts on output;
  diffuse textures allocate `GL_SRGB8_ALPHA8` (hardware sRGB→linear on sample); shader stays
  linear and never branches on missing vertex data.
- `finalAlpha = effectiveAlpha * sampledAlpha`; `effectiveAlpha = baseColor.a * opacity`.

## Asset & texture pipeline (ADR-0002/0007)

`manifest.geometry → OBJ → mtllib → MTL → map_Kd → diffuseTextureRef → Texture::fromFile()`.
Reference PNGs (`assets/references/**`) are metadata-only and never uploaded. Manifest paths and
MTL `map_Kd` share the same path-safety predicate: relative, no `../`, no absolute; a rejected
texture ref promotes the asset to `LOADED_WITH_WARNINGS` (geometry still renders with baseColor).

## Next steps

1. **M5 ImGui editor** (overlay owns its GL state; ADR-0005 handoff point =
   `Renderer::End3DPass()`; `lib/imgui` already fetched) — next milestone. Architecture accepted in
   **ADR-0011** (docked layout, explicit `ViewportRect`, `Editor` owning the `Scene`, input-ownership
   gate incl. the wheel); execution tickets **0028–0034** written; 0028 implemented, manual
   evidence pending.
2. **Pending follow-ups**: **0026** M5 camera interaction semantics & cursor baseline (M5-adjacent;
   blocked by 0028, does **not** block the M5 close), **0027** asset CPU/GPU boundary gate
   (**before M6**; premise corrected — the boundary already exists, what remains is header/type
   coupling).
3. **M6 Save/Load** — Scene JSON as a complete snapshot (forward contract in ADR-0010 Q4).