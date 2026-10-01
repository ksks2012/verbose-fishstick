# CHANGELOG

## [Unreleased] — M6 Save/Load (complete; 0041–0046 archived; two manual boxes open as an environment limitation)

### M6 — implemented and closed (0041–0046)
- **0046 — M6 close: integration/regression matrix, two-tier evidence, archiving.** The milestone
  closes on a new `scripts/m6-close-matrix.sh` (**71 checks, ALL PASS in dev and release**) with
  `scripts/m5-close-matrix.sh` left **byte-for-byte unmodified** and still ALL PASS — the ticket
  locked "a new script, not a grown M5 script", so "the pre-existing rows are unchanged" is
  verified by running both, not by reading a diff of a monolith. ctest **34/34** dev + release
  (27 → 34: one row per `--scene=` structural-failure case plus `--save-scene=` empty), 0 warnings.
- **The load moved PRE-GL**, which is what let the ticket's "no display required for any row" hold
  literally instead of being reinterpreted. `main()` reads and deserializes the document before
  GLFW is initialised and threads the result in as a separate `std::optional<scene::Scene>`
  argument rather than through `Options` (0025 froze `Options` as pure syntax; a deserialized Scene
  is runtime state). **This retracts a false constraint recorded in 0045** — that the load had to be
  post-GL "because an entry Scene needs the `AssetRegistry`, whose GPU cache requires a live
  context". `Scene` is pure data; the registry is only needed later to *resolve* assets into
  `DrawItem`s. The corrected rule is **load pre-GL, resolve post-GL**, and the stale claim in a
  `CMakeLists.txt` comment was corrected rather than left to mislead.
- **TDD at the seam removed a false-green assertion that predated this milestone**: the
  round-trip's "transform survives" check asserted `position.y == 0.0f`, which is also what a
  default-constructed transform holds — a writer that dropped every transform would have passed it.
  It is now a non-identity position/rotation/scale asserted exactly, plus a save→reload→save
  byte-identity check; red was proven by mutating the serializer to drop the position.
- **`/code-review` found a bug no row could see**: `buildEntryScene` ignored its own
  `preloadedScene` parameter and re-read the file post-GL — the exact behaviour 0046 exists to
  remove — and **every matrix row still passed**, because re-reading a *valid* document is
  indistinguishable from having preloaded it. The root cause was the **fallback branch** around the
  parameter: it reads as defensive programming, but it is where a broken preload hides, because it
  turns the failure mode into a success. It is removed; the parameter is required (no default) and
  the branch now fails loudly with an explicit "this is a bug, not a bad document" diagnostic. Two
  matrix rows assert the **mechanism**, not the output.
- **Nine injected defects each fail the matrix for their own reason** (load moved post-GL, the
  exclusion gate disabled, `M4_INFO` identity changed from stem to full path, the `(M4)` tag
  drifting, `<filesystem>` added to `editor.cpp`, a `gl*` symbol defined in the serializer, a
  dropped transform). The 0041 arch guard is re-confirmed **and proven to still fire**: an injected
  `renderer` include aborts configure, and an injected `gl*` symbol makes the symbol check report 1.
- **One ticket number did not hold and is amended in the ticket, not quietly restated**: the
  invariant "CTest count rises from 22" was already stale — 0045 had moved it to 27 before M6 work
  began. Recorded in the archived ticket's *Delivered* section.
- **Manual tier NOT claimed.** Opening a saved document in a real window and the `xprop` `WM_NAME`
  titles for a `--scene=` run need a window session this environment does not have; both are
  recorded as an environment limitation, per 0033's discipline. Summary:
  `docs/summary/0046-m6-save-load-close.md`.
- **0041–0045** (arch guard, `Scene::fromEntities` + `Editor::replaceScene`, the serializer
  round-trip, strict-structure/lenient-reference load policy, CLI `--scene=`/`--save-scene=` with
  app-layer atomic I/O) are complete and archived; the target graph gained one edge
  (`noto3d-editor → noto3d_serialization`) and no existing edge changed.

### Architecture — M6 Save/Load (grilling → ADR-0013)
- **ADR-0013 locked; executed by 0041–0046, all complete and archived.** A grilling session (Q0–Q11) settled the
  milestone before the first ticket could decide any of it by accident. M6 is a **persistence core +
  CLI round-trip**, not an in-editor document editor — `lib/` has no file-dialog library, and ADR-0003
  constrains new dependencies, so the UI affordances (file menu, picker, dirty state, Save-vs-Save-As)
  are explicitly deferred rather than smuggled in. A new `noto3d_serialization` target delivers
  `Scene` ↔ a single versioned `.scene` JSON document into the `src/serialization/` directory ADR-0008
  reserved at M0 and that has been empty ever since. The decisions most likely to be re-opened later:
  the **id allocator is derived, not persisted** (`max(restored)+1`), so a destroyed high-water id may
  reappear after a round-trip — accepted because a load starts a new Scene lifetime, nothing in M6
  observes the repeat, and it keeps the file a pure function of its entity set (this also amends
  `DEFINITION.md`'s overstated "runtime = persistent" wording, and is pinned by a test); and the
  **strict-structure / lenient-references** split, where a malformed or non-v1 document hard-fails
  loudly but an unresolvable `assetId` is kept, warned about, and skipped at render, so the M4
  "unresolved never invalidates the Scene" contract survives into persistence. Load reaches the
  editor through one new by-value `Editor::replaceScene(Scene)`, keeping the M5 guarantee that no
  caller can obtain a mutable `Scene&`/`Entity*`; save needs no new API. A configure-time arch guard
  makes the serializer's blindness to the asset registry structural instead of conventional.
  `--scene=<path>` is a third mutually exclusive entry mode (**superseding** M4's frozen "no
  `--scene=`", amended not edited), and `--save-scene=<path>` is an orthogonal output flag that
  writes atomically before the render loop. Amends ADR-0010 + `DEFINITION.md`; supersedes no prior
  decision. See `docs/adr/0013-scene-save-load-persistence.md`.

### Architecture — asset CPU/GPU boundary (0027 spike → ADR-0012)
- **0039 — asset CPU/GPU split close-out: graph recorded, full regression re-run.** The last tracer
  ticket, and the explicit "did the refactor change anything observable?" check. Its top-level
  invariant — **zero externally observable behaviour change** — is verified two ways. *Structurally:*
  the whole 0036–0039 chain touched **no** file in `src/scene/`, `src/app/draw_list.*`,
  `src/app/editor.*` or `src/ui/`, and every public declaration of `AssetRegistry` is byte-identical
  to pre-chain, so the M4 contracts (registry sole authority, `Scene` pure data, the
  `collectDrawItems` seam, the borrowed `DrawItem::model`) never had an opportunity to move.
  *Behaviourally:* clean dev + release builds at **0 warnings**, ctest **22/22** in both, both close
  matrices **ALL PASS**, the 0024/0033 CLI exit-code matrix unchanged, the GL-free link line and 0
  GL/glad symbols re-confirmed, and the manual tier-B `xprop` `WM_NAME` titles **actually executed**
  for all three entry modes on a real display (Weston/llvmpipe). The final target graph was read
  out of CMake itself with a `cmake_language(DEFER)` probe rather than transcribed from the ADR, and
  matches ADR-0012 ② including the 0037 amendment. No production code changed.
  Close-out audit residue is filed as **0040** (two ADR-0012 ①/③ invariants nothing asserts — one of
  which has a `check(... || true)` that can never fail). The 0036–0039 chain is now **complete and
  archived**, closing the 0027 spike.
  See `docs/archive/ticket/0039-asset-cpu-split-closeout.md`, `docs/summary/0039-asset-cpu-split-closeout.md`.
- **0038 — `tests/asset` and `--list-assets` stop linking GL** (ADR-0012 ③, tracer 3). The concrete
  win the 0027 spike was opened to price. `asset_registry.hpp` is now **GL-free** — it no longer
  includes `model.hpp`/`texture.hpp` and instead re-declares `renderer::Model`
  (`using Model = std::vector<SubMesh>` over an incomplete `SubMesh`) and `renderer::Texture`,
  which is sound because the registry only holds them behind `shared_ptr` and hands out borrowed
  pointers. `AssetRegistry` is split across two translation units: `asset_registry_cpu.cpp` (in
  `noto3d_asset_cpu`) defines `loadManifestOnly()`, `loadModelShapesOnly()` and every read-only
  accessor; `asset_registry.cpp` (in `noto3d_asset`) keeps `load()`, `loadModelFile()`, `reload()`
  and `uploadToGpu()`. The single mode-branching `doLoad(path, mode)` became two mode-specific
  drivers over **one shared** `loadManifestCpu()` pipeline, so the CPU half cannot drift between
  modes. `noto3d-registry-tests` now links `noto3d_asset_cpu` (+ `noto3d_core` for the `log.hpp`
  facade, the same test-only trade `noto3d-manifest-tests` makes) and its link line contains **no
  `noto3d_renderer`, no `glad`, no `libGL`** in dev or release. `runListAssetsCommand()` resolves its
  listing through `parseManifest()` + `loadManifestCpu()` with no `AssetRegistry` in it at all;
  output diffed byte-identical (modulo timestamps) against the old implementation, both on the clean
  manifest and with a warning-producing probe entry — a `parseManifest()`-only version would have
  looked identical while silently dropping every per-asset `M3_WARN`. `reload()` deliberately stays
  in the facade (it must repeat a Full load, and ADR-0010 relies on it invalidating borrowed
  `DrawItem::model` pointers), so its two cases moved to a new GL-linked target
  `noto3d-registry-reload-tests`; **no assertion was dropped** — this is the ticket's own escape
  clause, and it amends the ticket's 21/21 count to **22/22**. A third configure-time guard (the same
  shared include-closure function, applied to the test) keeps the CPU suite honest: `noto3d_core`
  puts `lib/glad/include` on its include path, so only a closure probe separates "GL is on the path"
  from "GL is actually reached"; `#include <glad/gl.h>` and `#include <model.hpp>` both fail
  configure. `noto3d-editor`/`noto3d_app` link graphs unchanged. ctest **22/22 dev + release**, both
  close matrices ALL PASS, CLI exit-code matrix unchanged. Also removed a duplicated
  `target_link_libraries(noto3d_asset ...)` call left by 0037.
  **Code review (two axes) then found and fixed one real regression:** the GPU dedup probe had been
  ordered *ahead* of the `fallback_geometry` resolution, where the pre-split code resolved fallback
  first — so an entry that hit the dedup fast path silently stopped reporting an unresolvable
  `fallback_geometry` and stopped degrading `overall`. Unreachable on the frozen manifest and
  invisible to the GL-free suite, so nothing had failed; it is now pinned by
  `testGpuDedupOrderingKeepsFallbackWarning` (verified red-green). The review also extracted the
  ADR-0007 per-entry epilogue as `validateCpuRecord()` and the two load drivers' shared preamble as
  `runManifestPipeline()` (the "one pipeline" claim did not hold until then), and corrected a stale
  `21/21` in ADR-0012 plus this ticket's overstated "names unchanged" wording.
  See `docs/archive/ticket/0038-asset-cpu-no-gl-link.md`, `docs/summary/0038-asset-cpu-no-gl-link.md`.
- **0037 — create the GL-free `noto3d_asset_cpu` target** (ADR-0012 ②, tracer 2). The CPU asset core
  moves out of the GPU facade into `src/asset/asset_cpu.{hpp,cpp}`: `CpuObj`, the ADR-0007
  `isUnsafePath()` predicate (**one definition now shared by both sides of the split**),
  `parseManifest()`, the CPU diagnostics, the 0013 texture-ref scan, `aggregateResults()`, and
  `CpuCache` (canonical-path dedup, "do not cache FAILED" preserved). `noto3d_asset` keeps its name
  and its **public API byte-for-byte** — it now holds only `uploadToGpu()`, the GPU caches and the
  per-asset status bookkeeping, and is a facade over the CPU core. One authority, two libraries:
  the registry still owns both caches. `nm -C libnoto3d_asset_cpu.a` → **0** `gl*`/`glad` symbols,
  and the only `noto3d::renderer::` types it contains are `Vertex` and `Material`. The boundary is
  enforced by two configure-time arch guards (link-level, and a preprocessor include-closure guard
  whose `src/renderer/` check is an **allowlist**, so a new renderer header fails the build until
  classified). Both guards derive their inputs from the target's own CMake properties. ctest
  **21/21 dev + release**, both close matrices ALL PASS, `--list-assets` still `count=6` with no
  `libGL` on the binary. Two implementation-forced link additions are recorded as an ADR-0012
  amendment (`glm::glm` PUBLIC, `spdlog::spdlog` PRIVATE — the latter linked *directly* so
  `noto3d_core` cannot drag `lib/glad/include` back onto the CPU target). The code review caught a
  live bug here: without the `glm` link the CPU target was resolving a **system glm 0.9.9** against
  the project's pinned **1.0.1** — an ODR split across the boundary, hidden by a guard whose `-I`
  list had drifted from the real build. See `docs/archive/ticket/0037-asset-cpu-target.md`.
- **0036 — extract `renderer::Vertex` into a GL-free `vertex.hpp`** (ADR-0012 ①, tracer 1). The
  32 B `Vertex` layout (ADR-0006) moves from `mesh.hpp` to its own `src/renderer/vertex.hpp`;
  `mesh.hpp` keeps only the GL-owning half (`Attrib` `GLuint` locations, `Mesh` VAO/VBO/EBO) and
  includes it. **No type renamed, no namespace change, no `using` alias** — the CPU asset parse
  header `obj_loader.hpp` now names `Vertex` via `vertex.hpp` instead of reaching `glad/gl.h`
  through `mesh.hpp`. Verified by include-closure: at HEAD `obj_loader.hpp` pulled 2 glad headers
  (`glad/gl.h`, `KHR/khrplatform.h`); after the change, 0. `asset_registry.hpp` still reaches glad
  by design — it is the GPU-facing facade exposing `renderer::Model*`, and ADR-0012 ② keeps it
  there permanently (only the CPU half becomes GL-free). Builds clean with 0 warnings, **ctest 21/21
  dev + release**, and both `m5-close-matrix.sh` runs ALL PASS. Also corrected a now-stale comment
  cross-reference in `texture.cpp` (the stride invariant moved out of `mesh.hpp`) and amended this
  chain's tickets: 0036's original static check (`grep "glad/gl.h" src/asset/*.hpp`) was
  **vacuous** — the literal string was never in the file, only in its transitive includes — so
  0037–0039 must use the closure form.
- **0027 — asset CPU/GPU boundary architecture spike. Closed.** The M6 gate is resolved: the spike
  confirmed the premise correction — **the runtime CPU/GPU boundary already existed and was already
  enforced** (`src/asset/` makes no direct GL call; `loadObjShapes`/`CanonicalShape`/
  `loadModelShapesOnly`/`loadManifestOnly` are CPU-only; `uploadToGpu()` is the single upload gate).
  What remained was header/type + **link** coupling, now decided in **ADR-0012** (no prior ADR
  amended; additive):
  - **① Type ownership** — `Vertex` (32 B) and `Material` are the interchange format, not GPU
    resources: `Vertex` moves to a new GL-free `renderer/vertex.hpp`, `Material` stays GL-free, same
    namespace / no rename. `Mesh`/`SubMesh`/`Model`/`Texture` stay GPU-owned (`Model` not moved).
    The CPU record's `Material::texture` is always `nullopt`; `uploadToGpu()` is its only writer.
  - **①b Lifetime** — unchanged and now explicit: `AssetRegistry` remains the **sole authority and
    sole owner** of both the CPU and GPU caches (a facade over two libraries, not two authorities —
    ADR-0007 verbatim); `DrawItem::model` stays the borrowed frame-local `const renderer::Model*`
    (ADR-0010 Q5), invalidated by `reload()`; the 0012 GL-owning-lifetime rule untouched.
  - **② Library shape** — a real split: new **`noto3d_asset_cpu`** (json + tinyobjloader only; no
    renderer/glad/libGL) holds the parse/dedup/manifest core; `noto3d_asset` (unchanged name +
    public contract) depends on it and keeps `uploadToGpu()`, the GPU caches, and all six
    `AssetRegistry` entry points.
  - **③ Consequences** — `loadManifestOnly()`/`loadModelShapesOnly()` kept as thin CPU-only paths
    (0025③); escape hatches + the 0013 path predicate unchanged; **`tests/asset` + `--list-assets`
    stop linking GL** — the concrete win the spike was opened to price. Rejected (with reasons in
    the ADR): single-lib GL-free header subset (kept as tracer 1), keep-as-is, renderer-side upload
    (inverts M4), split CPU/GPU `Material`, asset-owned `Vertex`/`Material` copies.
- **Execution** — four CPU-first tracer tickets, each independently green and behaviour-neutral:
  **0036** extract `renderer/vertex.hpp` · **0037** create `noto3d_asset_cpu` · **0038** retarget
  `tests/asset` + `--list-assets`, prove the GL-free link · **0039** close-out (graph record + full
  regression + docs). These are code-quality and do **not** block M6 scope. **This spike wrote no
  production code**; 0025 stays verification-only for ③/⑤.
  **All four complete 2026-09-29; the chain is closed and archived** — see the 0039 entry above.
  The 0037 amendment it delivers: `glm` PUBLIC and `spdlog` linked *directly* on
  `noto3d_asset_cpu`, never via `noto3d_core`, whose PUBLIC `lib/glad/include` would otherwise put
  glad back on the CPU include path.

### M5 — close (0033)
- **0033 — M5 integration/regression matrix + manual docking smoke.** Closed the M5 editing surface
  as a whole, mirroring 0024's role for M4 (ADR-0010 Q8 / ADR-0011 Q8 shape): no readback,
  screenshot comparison, or image similarity — an automated matrix (exit codes + structured
  runtime-log assertions + CTest) plus manual, platform-specific evidence recorded in the ticket.
  `scripts/m5-close-matrix.sh <build-dir> [--release]` runs the full two-tier check: CLI exit-code
  matrix (pre-GL rows assert exit codes on both configs; GL rows assert exact exit codes on release
  and log-line content on dev, where the documented Mesa llvmpipe LeakSanitizer 112-byte leak makes
  GL runs exit 1 while still emitting correct structured logs); structured-log content + ordering
  (`M0_OK` → `M3_INFO`/`M2_OK` → `M4_INFO entities=N` → `M1_DONE`) for default / `--asset=smile`
  (also `crown`) / `--demo`; `--list-assets` output frozen; the headless app suites asserting the
  M5 op logs directly via capture sinks. **M5_SEL coverage added** (`tests/app/editor_test.cpp` now
  asserts the selection line `M5_SEL entity='…' id=N` and that deselect stays silent) — the one M5
  op not previously wired into a log assertion. Static contract checks: `collectDrawItems` remains
  the sole render entry with resolver-only `registry.model(` calls; `DrawItem::model` is still a
  borrowed pointer; `noto3d_app` links neither ImGui nor GLFW.
- **M4 contracts re-verified unchanged:** the six CLI exit-code rows, `M0_OK`/`M3_INFO`/`M4_INFO`/
  `M2_OK` content + ordering, `--list-assets`, and `xprop` `WM_NAME` titles
  (`m4_vertical_scene | noto3d (M4)` / `<id> [placeholder] | noto3d (M4)` /
  `demo_checker [placeholder] | noto3d (M4)`) all match M4 exactly — the editor was added beside the
  runtime, not over it.
- **Result:** matrix green on dev + release (release 30/30 checks), ctest **21/21 dev + release**
  0 warnings on full rebuilds, 0028–0034 archived under ADR-0011, execution summary under
  `docs/summary/0033-m5-close-integration-regression.md`. **Open:** the four manual docking/edit
  smoke boxes (docking resize + non-square aspect, full editing pass, interaction ownership,
  no-gizmo/no-undo confirmation) require a real window session and are recorded as an open follow-up
  in ticket 0033 — not automatable in this environment.

### M5 — implemented (0028, 0029)
- **0028 — editor shell, `ViewportRect`, input-ownership gate.** Docked host with the 3D view as a
  real panel; one plain-data framebuffer-space `ViewportRect` as the single source of view extent;
  a four-input app-layer gate (`viewportHovered && !IsAnyItemHovered() && !IsAnyItemActive() &&
  !globalModalCapture`) covering the wheel as well. Headless suites `app-viewport-rect-math`
  (37 checks) and `app-input-gate-contract` (17 checks, full 16-combination sweep) link
  `noto3d_core` only. Manual docking evidence still pending.
- **HiDPI correction.** The first viewport conversion built GL coordinates from ImGui screen
  pixels plus framebuffer height, silently ignoring display scale and viewport origin. Fixed with
  `toFramebufferRect(ScreenRect, DisplayTransform)`; scale is derived from the GLFW
  framebuffer/window ratio because the GLFW backend reports `DisplayFramebufferScale` as `1.0` on
  X11. Regression cases cover non-zero host origin, anisotropic scale, `lround` rounding, and a
  zero-height (minimised) framebuffer.
- **0029 — Hierarchy, selection, `Editor` facade.** `app::Editor` owns the `Scene` **by value** and
  is the sole UI write path: its public API returns no mutable `Scene&`, no `Entity*` and no entity
  span, so the Q3 `EntityId`-only rule is a type boundary rather than a convention. Selection is
  editor-session state (`EntityId`, re-resolved per frame) held separately from the per-view
  `ViewportState` and per-frame `InputState` (Q6). Structural mutations clear a stale selection
  explicitly. Headless `app-editor-facade` suite, 52 checks, links `noto3d_app noto3d_core` only.
  Rendering remains the single `collectDrawItems(editor.scene(), resolver)` seam.
- **Structural pass.** `main.cpp` reduced 611 → 256 lines by extracting `src/app/viewport_rect`,
  `src/app/input_gate`, `src/app/camera_input` (ImGui-free logic) and the CLI→Scene entry model,
  plus presentation/input adaptation under `src/ui/`. All headers are declaration-only — every
  definition lives in a `.cpp`. M4 contracts re-verified unchanged: `M4_INFO` for all three entry
  modes and the six CLI exit-code cases.
- **ctest 18/18 on dev and release**, 0 compiler warnings in our own code both ways.
- **Open:** manual evidence for both tickets. For 0029 this is not a formality — `ImGui::Selectable`
  needs a real pointer, and a selection click makes `IsAnyItemActive()` true, which closes the
  camera gate for that frame; that interaction is currently unverified.
- **Raised — 0035 (declaration-only conformance).** ADR-0008 requires headers to carry declarations
  only. Ten definitions still violate it: `camera.hpp` (4), `mesh.hpp` (2), `texture.hpp` (1),
  `panel_render.hpp` (1), `transform_edit.hpp` (2). Two of them — `Texture::id()` and
  `PanelViewport::active()` — have zero call sites and are to be deleted rather than moved. Seven are
  pre-existing renderer code; one came from 0028's `src/ui/` extraction; two came with 0030's
  `transform_edit.hpp`. The verification note in
  ticket 0028 claiming "all headers are declaration-only" is **false** and is corrected via 0035;
  it was produced by a keyword-based grep that cannot see implicitly-inline member definitions.
  Independent of the M5 chain; does not block 0030.
- **Corrected — "0 warnings" claim in 0028/0029 sites.** A full clean rebuild (incremental builds
  never re-emit an already-compiled file's warnings) exposes **one** warning from `a05e852`
  (0028): `main.cpp:227` ignored the `[[nodiscard]]` result of
  `ui::installGlDebugCallback()`. Fixed by an explicit `static_cast<void>` plus a comment
  documenting why it is safe to discard: the callee self-reports, and in a release build `false`
  means "debug GL never requested", not failure. Verified gone on full dev + release rebuilds.
- **Open — deviation from ADR-0011 Q5.** The 0028 implementation put `src/ui/` in a new
  `noto3d_ui` static library, but Q5 states those sources compile into the existing `noto3d-editor`
  executable and rejects a separate library for M5 (the adapter needs a display and a GL context, so
  a new target "buys no verification"), with revisit gated on a second host — which does not exist.
  Recorded rather than papered over; see the 0028 ticket. Resolution is either folding the five
  `src/ui/` sources back into `noto3d-editor`, or an ADR-0011 amendment.

### M5 — implemented (0030, 0031)
- **0030 — Inspector numeric transform editing.** Inspector shows Pitch/Yaw/Roll (degrees) + scale
  for the selected entity, applied in the scene's radians YXZ order (fields labelled by axis with
  the applied order shown). The `Scene` stays radians; degrees are a presentation-layer conversion
  whose source of truth is the scene value each frame, with the displayed precision authoritative on
  commit (ADR-0011 Q13). Commits use `IsItemDeactivatedAfterEdit()` (continuous-widget commit
  boundary, Q8) and log `M5_XFRM` on the event. Writes go through `app::Editor`'s EntityId-keyed
  operations, so the four-input camera gate keeps arbitrating (widget-active release returns the
  wheel to the camera on the same frame the editable goes inactive). Headless `app-transform-edit`
  suite (was `app-editor-facade`-adjacent) covers degree/radian boundary values, display-precision
  commitment, radians YXZ application order, scalar/vector commits and the `M5_XFRM` log lines
  (position/rotation/scale), linking `noto3d_app noto3d_core` only.
- **0031 — M5 asset picker.** Inspector `Asset` combo lists only drawable (renderable) assets for the
  selected entity, rebuilt from the registry's asset list as a **value snapshot** when the selection
  changes; the UI never holds a `std::span`/pointer into registry storage, and the identical 6-entry
  run is normal (no cache/version token, Q7). Reference-only / geometry-less entries are hidden and
  the hiding is observable: `M5_ASSET asset-picker n=<renderable> hidden=<reference-only>`. The
  snapshot carries **typed** values (id + `GeometrySource`); display text lives in the UI adapter.
  Choosing an entry commits through `commitAssetAssignment` → `Editor::setEntityAssetId` (the sole
  Scene effect, `EntityId`-keyed `assetId` mutation, ADR-0011 Q1) and logs
  `M5_ASSET asset-assigned entity='…' id=<n> assetId='…'` on the discrete selection event (never per
  frame, Q8). The sanctioned `noto3d_app → noto3d_asset` edge promotion happens here (per the CMake
  ownership comment). Headless `app-asset-picker` suite covers snapshot construction from a mixed
  manifest (renderable + reference-only), typed choice tagging, panel creation, editing, and that
  the effect on the Scene is limited to `assetId` (two entities on one asset stay independent; an
  unresolvable id degrades to the existing skip-not-fail seam). Suites 18 → **20**; ctest 20/20 on
  dev and release, 0 warnings on full rebuilds.

### M5 — implemented (0032)
- **0032 — M5 create/destroy entities.** Completes the editing surface: the Hierarchy pane gains a
  Create control and a Destroy button. Create's asset choice comes from the **same picker choices**
  the 0031 snapshot offers, so a new entity is drawable by construction — it never starts life
  unresolvable (ADR-0011 Q7; new entity name defaults to its asset id; create does not auto-select).
  Destroy removes the currently selected entity via the facade and clears the selection. Both go
  through `Editor` semantic operations keyed by `EntityId` (Q3/Q11) — no undo, no destroy+recreate
  fake-undo, and `EntityId` stays monotonic/never-reused with the counter preserved across
  `clear()` (ADR-0010 Q2/Q4, M6 identity contract). No `Scene` revision counter and no prefab/scene
  graph/entity-type concept were introduced (Q6, M4 exclusions). App-layer commit utilities live in
  new `src/app/entity_edit.{hpp,cpp}`: `commitCreateEntity` and `commitDestroySelected`, each
  logging `M5_ENTITY created entity='…' id=N assetId='…' count=M` / `M5_ENTITY destroyed
  entity='…' id=N count=M` on the discrete event (Q8; destroy-with-nothing-selected is a silent
  safe no-op). Headless suite `app-entity-edit` covers create-through-facade + choice-carrying +
  selection untouched, and destroy-of-selection incl. no-op paths; `app-editor-facade` grew
  facade-level id-monotonicity-across-cycles and no-op destroy checks. Suites 20 → **21**; ctest
  21/21 on dev and release, 0 warnings on full rebuilds.

### M5 — editor architecture decided (grilling Q0–Q14; spec embedded in ADR-0011)
- **M5 architecture accepted before any code** in **ADR-0011** (spec at the ADR layer per
  DOCUMENTATION-LIFECYCLE; the M5 execution tickets are **0028–0034** and nothing here is a behaviour
  change). Decisions locked:
  - **Viewport geometry (Q0)** — docked layout; one explicit framebuffer-space `ViewportRect` as
    plain data (no ImGui types) and the single source of view extent. It drives projection aspect,
    cursor window→viewport-local mapping with containment, and the app-established
    `glViewport`/scissor before the 3D pass. Replaces M4's implicit "the framebuffer *is* the
    view". `Renderer::Begin3DPass()` stays UI-agnostic and takes no rect.
  - **Editor ownership (Q11)** — an app-layer `Editor` **owns the `Scene` by value** and exposes only
    `EntityId`-keyed semantic operations. The UI receives the facade and therefore **structurally
    cannot** obtain a mutable `Scene&`, `Entity*` or span — the `EntityId`-only write path (Q3) is
    a type boundary instead of a convention. Rendering remains `collectDrawItems(editor.scene(),
    resolver)`: no second resolution path, single M4 seam, borrowed `DrawItem::model` untouched.
    Owning the `Scene` also gives M6 a ready-made snapshot-replace boundary.
  - **Edit semantics (Q3/Q4)** — editor state holds `EntityId` only and re-resolves every frame;
    mutations go through thin app-layer operations. **No undo/redo**: no command history, no
    apply-on-release, no drag coalescing, and destroy+recreate is explicitly **not** an undo
    (ids are monotonic and never reused, so it would break M6's single-identity contract).
  - **Testability (Q5)** — selection, edits, hierarchy, asset filtering and viewport geometry are
    ImGui-free and live in `noto3d_app` (which links neither ImGui nor GLFW, a property that
    already holds and is now load-bearing), so they extend `tests/app/`. `src/ui/` is
    presentation/input adaptation only; its sources compile into the existing `noto3d-editor`
    executable (a new library target was rejected as speculative).
  - **State lifetimes (Q6)** — split three ways: `EditorState` (session: selection),
    `ViewportState` (**per view**: `Camera` + `ViewportRect`; one view ships, per-view shape so a
    split view is an instantiation later), `InputState` (per frame). No Scene revision counter at M5.
  - **Input ownership (Q9/Q10)** — a binary gate:
    `viewportHovered && !IsAnyItemHovered() && !IsAnyItemActive() && !globalModalCapture`, with the
    **wheel under the same gate** and `NoScrollbar|NoScrollWithMouse` on the viewport window. The
    current runtime applies camera input unconditionally (no containment, no ImGui check,
    unconditionally accumulated scroll), so panels would otherwise move the camera and panel
    scrolling would zoom. Recorded as a **non-obvious constraint**:
    **`!io.WantCaptureMouse` must never be used as the viewport camera gate** — the viewport is
    itself an ImGui window, so that spelling makes camera input permanently dead, and ImGui's own
    docs point that way. `WantCaptureMouse` is reserved for genuinely global modal/popup capture.
    The follow-on "camera is frozen during a widget drag, so a baseline must be re-established on
    drag end" is a **baseline** problem owned by 0026, not an ownership one.
  - **Assets (Q7)** — the picker consumes an app-built value snapshot (never a `std::span` into
    registry storage, which reload invalidates); renderable-only, with hidden counts logged; no
    cache/generation; effect on the Scene is `assetId` mutation only.
  - **Observability (Q8)** — continues the structured-log pattern; continuous widgets commit/log on
    `IsItemDeactivatedAfterEdit()`, discrete widgets on the event. The frozen `WM_NAME` title
    contract and the no-readback/similarity exclusion both continue.
  - **Transform presentation (Q13)** — `Scene` keeps radians YXZ (unchanged); degrees are a
    presentation-layer conversion in `src/ui/`, by the same principle that keeps pixel→radian
    mapping in the app/input layer. Fields labelled by real meaning (`Pitch (X)` / `Yaw (Y)` /
    `Roll (Z)`) with the applied order Y→X→Z shown, because storage letter order is *not*
    application order (`eulerAngleYXZ(y, x, z)`). The Scene value is the widget's per-frame source
    of truth (no persistent degrees copy), and displayed precision is authoritative on commit
    (WYSIWYG) — stated so the resulting quantisation is a contract, not a bug report.
  - **Scope exclusions (Q4/Q12/Q1)** — undo/redo; **gizmos / on-viewport direct manipulation**
    (a gizmo would add a third input owner and turn the boolean gate into a state machine, plus
    picking and handle layout — deferred to a later direct-manipulation milestone); and the asset
    CPU/GPU refactor.
- **ADR-0004 amended** — the viewport is no longer identical to the framebuffer; projection aspect
  comes from the viewport rect. No GL capability, sRGB, MSAA or init-order decision changes.
- **ADR-0005 amended** — pass ownership extends to the app establishing `glViewport` + matching
  scissor before the 3D pass (so the clear is confined to the 3D view), and wheel ownership follows
  the pass contract. `Renderer` gains no parameters and no geometry concept.
- **ADR-0010 unchanged** — its Q1/Q3/Q5/Q6/Q7/Q8 contracts are all preserved, stated explicitly in
  ADR-0011 as an auditable claim.
- **0026 re-scoped and made active in M5** — blocked by the first slice's input gate, and narrowed
  to camera interaction semantics, wheel routing, sensitivity config (still **not** in
  `renderer::Camera`) and cursor-baseline re-establishment. Input ownership is no longer re-decided
  there. Two implementation facts corrected in the ticket: `prevX/prevY` update **every frame**
  (there is no per-drag stale delta; the issue is baseline re-establishment at ownership
  transitions), and the sensitivity constants already live in the app/input layer, so the ADR-0010
  Q7 purity boundary currently holds.
- **0027 re-scoped to an M6-preceding gate** — premise corrected: a CPU/GPU boundary **already
  exists** (no direct GL calls in `src/asset/`; `loadObjShapes`/`CanonicalShape`/
  `loadModelShapesOnly` are CPU-only; `uploadToGpu()` is the single upload gate) and what remains
  is header/type coupling (`AssetRegistry` exposes `renderer::Model*` and caches a `Texture`). M5
  binding invariant: no new `Model`/`Mesh`/`Texture`/GPU ownership; asset editing mutates a stable
  `assetId` string only.
- **Execution tickets 0028–0034 derived from ADR-0011** (tracer bullets, each independently
  demoable, with declared blocking edges): 0028 docked shell + `ViewportRect` + input-ownership gate
  (frontier root) → 0029 Hierarchy + selection + `Editor` facade → **0030** Inspector transform
  editing and **0031** asset picker in parallel → 0032 create/destroy (needs 0031's asset choice,
  rather than a "first renderable" default) → 0033 M5 close (matrix + manual docking smoke, blocked
  by 0030 + 0032). 0026 is re-gated onto 0028 as **M5-adjacent and not blocking the close**.
  No M5 code has been written.
- CONTEXT.md updated in the same change.

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