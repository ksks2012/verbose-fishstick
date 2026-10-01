# noto3d — Noto 3D Emoji Editor (MVP)

C++/OpenGL editor MVP for Noto 3D-style emoji composition. With M0–M6 complete this is a
docked editor built on a real Scene engine, with scenes you can save and reopen: an asset pipeline
plus a minimal windowed renderer (manifest assets the user picks, plus a textured demo fixture), a
pure-data Scene model with multi-entity composition + transforms + camera (M4), a full ImGui editing
surface — select, transform, swap asset, create, delete — with end-to-end structured observability
(M5), and versioned `.scene` JSON snapshots with atomic writes and a CLI round-trip (M6).

- **Language / API:** C++20, OpenGL **3.3 Core** / GLSL 330, GLFW 3.4 (X11), GLM,
  **ImGui (docking)**, nlohmann/json, tinyobjloader (OBJ/MTL), spdlog (Log backend),
  glad + stb_image (committed under `lib/`)
- **Docs hierarchy:** `docs/CONTEXT.md` = current implementation truth; `docs/adr/` = decision
  history (+ embedded milestone specs); `docs/DEFINITION.md` = project vocabulary;
  `docs/CHANGELOG.md` = milestone history;   `docs/archive/ticket/` = all completed execution
  tickets (incl. the M6 chain 0041–0046 and the 0027 asset-boundary spike 0036–0039);
  `docs/ticket/` = pending (0026 M5-adjacent camera interaction; 0035 header conformance;
  0040 asset-CPU invariant assertions; **0047 the manual verification tier that M5 and M6 could not
  complete here**).

## Status

| Milestone | Scope | State |
|---|---|---|
| **M0** | Build/env baseline (0001–0004) | ✅ complete — green configure/build/verify on dev, dev-clean, release |
| **M0.5** | Asset contract spike (manifest + placeholders + references) | ✅ complete |
| **M1** | Minimal renderer (0005–0007) | ✅ complete — Mesh upload, Material/shader/first draw, pass hygiene |
| **M2** | Asset pipeline (0008–0014) | ✅ complete — `AssetRegistry` + OBJ/MTL→Mesh, sRGB diffuse texture pipeline + E2E demo render, demo fixture, texture path safety |
| **M3** | Noto vertical slice (0015–0018) | ✅ complete — fixture frozen (manifest-integrity contract test, ctest 5/5), selection diagnostics (`M3_INFO` identity line, dynamic title, `--list-assets`), human category verification |
| **M4** | Scene composition & transform (0020–0024) | ✅ complete — pure-data Scene + draw-list seam + multi-entity runtime + orbit/zoom/pan camera; automated matrix + xprop titles + smoke; 0025 code-cleanup pass complete (ctest 15/15) |
| **M5** | ImGui Editor (0028–0034) | ✅ complete — docked shell + `ViewportRect` + input-ownership gate, Hierarchy/selection/`Editor` facade, inspector numeric transform editing (degrees → scene radians YXZ), asset picker (value snapshot), create/destroy; closed on the automated close matrix (ctest 21/21 **at M5 close**; the suite is now 34/34, `scripts/m5-close-matrix.sh`) + xprop titles. Manual docking/edit smoke open as a follow-up (real window session required) — tracked in **0047** |
| **M6** | Save/Load (0041–0046) | ✅ complete, machine-verified — versioned `.scene` JSON snapshots of a Scene, `Scene::fromEntities` restore with a derived id allocator, strict-structure/lenient-reference load, `--scene=` as the third pre-GL entry mode, `--save-scene=` with atomic writes. Closed on `scripts/m6-close-matrix.sh` (71 checks dev + release) + ctest 34/34, 0 warnings. The one production change this milestone needed (loading **pre-GL**, so a bad document costs no window) is a separate commit, `242ce64`. Manual tier **not claimed** — no window session here; tracked in **0047** |

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

M4 tickets (0020–0025), M5 tickets (0028–0034) and the M6 chain (0041–0046) are complete and
archived. **M5's architecture was locked in ADR-0011** (docked editor layout, `ViewportRect`,
`Editor`-owns-`Scene` facade, input-ownership gate incl. the wheel) and its execution tickets shipped
as **0028–0034**; the M5 close is delivered by **`scripts/m5-close-matrix.sh`** (CLI exit-code +
structured-log matrix for every entry mode, M5-op log assertions via the headless app suites,
static seam checks) with **ctest 21/21 dev + release at the time** (the suite is now 34/34) and `xprop` titles re-verified unchanged from
M4.

**M6's architecture was locked in ADR-0013** and shipped as **0041–0046**: a GL-free
`noto3d_serialization` target behind a configure-time arch guard, `Scene::fromEntities` as the
restore entry, a strict-structure / lenient-reference load policy, and the CLI surface
(`--scene=`, `--save-scene=`) with app-layer atomic file I/O. Its close is delivered by
**`scripts/m6-close-matrix.sh`** (71 checks, dev + release) with **ctest 34/34 dev + release** and
**`scripts/m5-close-matrix.sh` deliberately unmodified** — 0046 locked "a new script, not a grown
one", which is what makes "the pre-existing rows are unchanged" cheap to verify by running both.

**Manual verification is outstanding for both milestones and is tracked in `docs/ticket/0047`.** The
automated tier cannot establish the `xprop` `WM_NAME` strings, the docking/viewport interaction
ownership, or what a reopened document actually looks like — those need a display. M5 and M6 are
therefore **machine-verified, not fully verified**; the distinction is deliberate, and a reader
should not infer the stronger claim from the green matrices. **0026** (M5 camera interaction
semantics & cursor baseline, M5-adjacent, waiting on 0028) and **0035** (header conformance,
code-quality) are the other pending side-tickets; the **0027** asset CPU/GPU boundary spike is
**complete (ADR-0012)** with its implementation chain **0036–0039 complete and archived**, leaving
**0040** (two ADR-0012 invariants nothing enforces) as its close-out residue. All completed
execution history lives under `docs/archive/ticket/`. Current milestone: **M6 Save/Load complete
and machine-verified**; no M7 is scoped.

## Build & test

Machine prereq (one-time): `sudo apt install libxinerama-dev libxcursor-dev libxi-dev`

```bash
cmake --preset dev                      # configure (Debug + sanitizer option)
cmake --build build/dev -j               # build
ctest --test-dir build/dev               # headless tests (34 suites)
build/dev/noto3d-editor --frames=5       # non-interactive smoke: prints M0_OK / M1_DONE
build/dev/noto3d-editor                  # interactive window

# M6: entry modes and snapshot I/O (--scene= and --asset=/--demo are exclusive).
# --frames=N bounds the run; without it the window opens and waits for input.
build/dev/noto3d-editor --list-assets                        # pre-GL, no display needed
build/dev/noto3d-editor "--scene=my.scene" --frames=8        # open a snapshot
build/dev/noto3d-editor --demo --frames=8 "--save-scene=out.scene"   # render, then save atomically
```

A structural problem in a `--scene=` document is a **user error, not a rendering error**: it exits
non-zero with a diagnostic naming the defect, **before any window or GL context is created**, so
those cases need no display. An unresolvable `assetId` is the deliberate exception — the entity is
kept as data, warned about, and skipped at render (the M4 contract).

Saves are atomic: the temp file is created in the *destination* directory (so `rename()` is atomic),
overwrite is silent, and a missing parent directory is an error rather than a silent `mkdir`.

Structured-log prefixes: `M0_OK` (context/capability), `M1_OK` / `M1_DONE` (renderer, frame
completion), `M2_*` (mesh/texture), `M3_INFO` (asset identity), `M4_INFO` (scene about to render
— logs `entities=<N>`), the M5 editing ops asserted by the headless app suites: `M5_SEL`
(entity selection, `entity='<name>' id=<n>`), `M5_ASSET` (asset picker offered/hidden counts +
assignment), `M5_XFRM` (transform commits, rotation in degrees+radians), `M5_ENTITY`
(create/destroy, `count=<N>`), and the M6 event lines: `M6_INFO` (`scene-loaded` /
`scene-saved`), `M6_WARN` (unresolved `assetId`, warned not refused) and `M6_FAIL` (a structural
defect, naming the defect). A `--scene=` run **reuses the frozen `M4_INFO` line** rather than
introducing a new shape — its scene name is the **file stem**, and the milestone tag stays `(M4)`.

To re-run either close matrix on a build dir (the dev axis asserts log content; add `--release` for
exact GL exit codes, since the dev preset's LeakSanitizer reports a known Mesa llvmpipe leak):

```bash
scripts/m5-close-matrix.sh build/release --release   # M4/M5 entry modes + headless suites
scripts/m6-close-matrix.sh build/release --release   # + M6: pre-GL failures, save, round-trip
```

Both are green in dev and release. `m6` is a **separate** script from `m5` by decision (0046): the
M5 script had already absorbed rows from two milestones, and leaving it byte-identical is what makes
"the pre-existing rows are unchanged" verifiable. The remaining manual boxes are in
`docs/ticket/0047`.

Presets: `dev` (Debug + sanitizer/debug-GL options), `dev-clean`, `release`. Our own code builds
with `-Wall -Wextra -Wpedantic`; `-Werror` is opt-in (ADR-0008).

## Layout

```text
assets/                 manifest.json (v1, authoritative) + references/ (official PNGs),
                        geometry/ (placeholder OBJs incl. demo_checker fixture)
src/                    app/ editor facade + entity/transform/picker edit logic + CLI entry
                        modes + scene-file I/O (app layer, the only filesystem-touching code) ·
                        ui/ ImGui presentation & input adaptation (docked editor shell) ·
                        scene/ pure-data Scene (EntityId/Transform/Entity) · core/ Log + GL
                        capability · renderer/ Mesh/Model/Material/ShaderProgram/Texture/Renderer ·
                        asset/ AssetRegistry+OBJ/MTL loader · serialization/ M6 .scene JSON
                        (GL-free behind a configure-time arch guard)
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

1. **Manual verification tier (0047) — the one thing genuinely outstanding.** M5's four
   docking/edit-smoke boxes and M6's two (open a saved document and confirm the entities and
   transforms; `xprop` `WM_NAME` for a `--scene=` run) need a real windowed session. Everything
   machine-checkable about both milestones is done and green; these are not, and no amount of
   headless evidence substitutes. Do not read the green matrices as "fully verified".
2. **Pending ticket work**: **0040** (two ADR-0012 ①/③ invariants the ADR states but nothing
   mechanically checks — code-quality, off-chain), **0035** (declaration-only header conformance: 10
   remaining inline definitions in `renderer`/`ui` headers — code-quality, off-chain), **0026** (M5
   camera interaction semantics & cursor baseline; M5-adjacent, still waiting only on 0028).
3. **No M7 is scoped.** The natural next milestone would be a document workflow in the editor
   itself (open/save from the UI, dirty state) — deliberately deferred out of M6 in ADR-0013
   because `lib/` has no file-dialog library and ADR-0003 constrains new dependencies. That would
   need its own grilling session and ADR before any ticket.

The two close matrices are the fastest way to re-establish that nothing regressed:
`scripts/m5-close-matrix.sh` and `scripts/m6-close-matrix.sh`, each against `build/release
--release`.