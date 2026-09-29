# ADR-0012 — Asset CPU/GPU boundary: `noto3d_asset_cpu` + `noto3d_asset`

Status: Accepted (spike, ticket 0027; execution tickets 0036–0039)

> Per DOCUMENTATION-LIFECYCLE: this ADR carries the **type-ownership and library-shape decision**
> for the asset CPU/GPU boundary (ticket 0027's spike). It **supersedes no earlier decision**;
> ADR-0007 (registry authority + escape hatches), ADR-0006 (vertex/material contract) and
> ADR-0008 (module boundaries) all stand unchanged. The execution (tickets 0036–0039) is a
> *relocation* of existing, already-accepted code — not a new architecture, and not a behaviour
> change.

## Context

Ticket 0027 was written on the premise that "asset manifest/OBJ parsing is fused to a GL upload
path". The 2026-09-28 spike (ADR-0011 Q1 premise correction) found that premise **out of date**:
**a CPU/GPU runtime boundary already exists and is already enforced**:

- `src/asset/` makes **no direct GL calls** anywhere.
- `loadObjShapes()`, `CanonicalShape`, `loadModelShapesOnly()` and `loadManifestOnly()` are
  already **CPU-only** (parse + dedup, no GPU resource).
- `uploadToGpu()` is already the single, explicit GPU-upload gate.

What actually remains is **header/type and link coupling**, not a missing runtime boundary:

1. **Header coupling.** `src/asset/obj_loader.hpp` includes `<material.hpp>` and `<mesh.hpp>` to
   name `renderer::Vertex` and `renderer::Material`. `material.hpp` is itself GL-free (glm only),
   but `mesh.hpp` carries `#include <glad/gl.h>` — so the *CPU* parse header transitively reaches
   `glad/gl.h` solely to reach the 32 B `Vertex` struct (ADR-0006).
2. **Type coupling.** `AssetRegistry`'s single public header exposes GPU types: `loadModelFile()`
   and `model()` return `const renderer::Model*`, and it caches `shared_ptr<renderer::Model>` /
   `shared_ptr<renderer::Texture>`. One type answers both the CPU question ("which assets exist?")
   and the GPU question ("what `Model` is this?").
3. **Link coupling.** `noto3d_asset` links `noto3d_renderer` **PUBLIC**; `noto3d_renderer` links
   `glad`. Therefore **every** consumer of `noto3d_asset` — including `tests/asset/registry_test.cpp`
   and the `--list-assets` CLI path — transitively links GL headers/`libGL`, even though the asset
   tests exercise *only* CPU paths today (all of them go through `loadManifestOnly()` /
   `loadModelShapesOnly()` and assert `model()==nullptr` / `externalModel()==nullptr`). 0025⑧
   recorded this reach as "expected today (0027)".

The question 0027 asks is a real one — should the link and the ownership be separated — but it is
**not a defect, not urgent, and not M5 work**. This ADR answers it so M6 (Save/Load) does not
proceed on an undetermined model. It decides **type ownership**, **CPU/GPU lifetime**, and
**library shape**, and prices the consequences.

---

## Decision

### ① Type ownership

- **Pure-data render types stay with the renderer as the *shapes* the pipeline agrees on, but they
  gain a GL-free home.** `Vertex` (32 B, ADR-0006) and `Material` (ADR-0006) are the interchange
  format between asset parsing and GPU upload, not GPU resources. Decision: `Vertex` moves out of
  `mesh.hpp` into a **new GL-free header `renderer/vertex.hpp`**; `Material` stays in the
  (already GL-free) `material.hpp`. The *namespaces do not change* — this is a **header
  relocation, not a type move**, so no consumer is renamed and no `using` alias is introduced.
  - `mesh.hpp` (GL-owning: VAO/VBO/EBO, `Attrib` `GLuint`s) keeps `Mesh` and `Attrib`, and
    includes the new `vertex.hpp`.
  - `CanonicalShape` (asset CPU record) continues to be `std::vector<renderer::Vertex>` + indices
    + one `renderer::Material`; its **types are unchanged**, only the include it now needs
    (`vertex.hpp`/`material.hpp`, both GL-free) is narrower.
- **GPU resource types stay unambiguously renderer/GPU-owned**: `Mesh`, `SubMesh`, `Model`,
  `Texture`. `Model` is *not* moved — it is a `vector<SubMesh>` of `Mesh` (a live GPU resource
  aggregate) and has no CPU meaning. No asset-domain type may expose or return one except the two
  documented `AssetRegistry` GPU entry points (`model()`, `loadModelFile()`), and `CpuObj` /
  `CanonicalShape` may **never** mention them.
- **`Material::texture` (optional GL texture name) remains a renderer field.** The CPU parse only
  ever sets `diffuseTextureRef` (a relative path); `texture` is filled by `uploadToGpu()` at upload
  time. The CPU record may therefore carry a `Material` whose `texture` is null — the invariant
  is: **`CanonicalShape::material.texture` is always `std::nullopt`** (GPU upload is the only
  writer). This is a stronger, checkable contract than "usually null".

### ①b CPU data and GPU resource lifetime

- **`AssetRegistry` remains the sole asset authority** (ADR-0007) and remains the **sole owner of
  both** the CPU dedup cache (`m_cpuCache`, shared immutable `CpuObj`) and the GPU dedup caches
  (`m_modelCache`, `m_textureCache`). Splitting the library does **not** move GPU ownership into
  `renderer` or a new `AssetUploader` — the registry is a **facade over two libraries**, not two
  authorities. This preserves ADR-0007 verbatim.
- **`Model` lifetime is unchanged and survives the split.** Uploaded `Model`s are owned by
  `shared_ptr` in the registry's caches; `DrawItem::model` stays the ADR-0010 Q5 **borrowed,
  frame-local `const renderer::Model*`**, valid only until the frame's draw-list is consumed and
  never cached by the app across frames or `reload()`. The split does not create, widen, or
  narrow this contract — the resolver seam (`app::collectDrawItems`) is untouched.
- **The GL-owning-lifetime discipline (destroy while the context is current, ticket 0012) is
  unchanged** — every `Mesh`/`Texture` is still owned by the registry (a local in
  `renderScene()`'s scope), destroyed before the context goes away. The split does not relocate
  any GL owner.
- **What invalidates a `Model*`:** a `reload()` (or a fresh `load()`) rebuilds the GPU caches, so
  a previously returned `Model*` is dangling afterwards. This is already true today and is
  explicitly restated here as the post-split answer.

### ② Library shape

Decision: **split `noto3d_asset` into two CMake targets** — a GL-free CPU core and a GPU facade:

- **`noto3d_asset_cpu` — no GL, no renderer.** Contains `manifest_types.hpp`, `obj_loader.{hpp,cpp}`
  (now including the GL-free `vertex.hpp`/`material.hpp`), and the pure parse/dedup/entry logic.
  It links only `nlohmann_json` (PUBLIC) and `tinyobjloader` (PRIVATE) — **no `noto3d_renderer`,
  no `glad`, no `libGL`**. This is the target `--list-assets` and `tests/asset/` build against.

  **Amendment (ticket 0037, implementation-forced).** The link set above is the *intent* — no GL,
  no renderer — but two header-only third-party libs also have to be linked, without weakening it:
  - **`glm::glm` (PUBLIC).** ① makes `Vertex`/`Material` the interchange format, and both are glm
    types. The CPU parse path therefore compiles glm-using code, so it must resolve glm. Without
    this link the CPU target silently picked up a **system glm (0.9.9) while the project pins
    `lib/glm` 1.0.1** — an ODR split across the boundary, with the 32 B `Vertex` static_assert
    checked against the wrong glm (ADR-0003 "external libs live in `lib/<name>`" + 0010's SHA
    pinning bypassed). glm is pure math: no GL, no renderer.
  - **`spdlog::spdlog` (PRIVATE).** `obj_loader.cpp` uses `NOTO3D_DEBUG` from `log.hpp`, so the log
    facade is reachable from the CPU target. `spdlog` is linked **directly rather than via
    `noto3d_core`** precisely to keep the boundary clean: `noto3d_core` exports `lib/glad/include`
    PUBLIC, so linking it would put glad's headers back on the CPU target's include path. The
    configure-time link guard therefore forbids `noto3d_core` by name.

  The net link set is therefore `nlohmann_json` + `glm` (PUBLIC), `tinyobjloader` + `spdlog`
  (PRIVATE) — still **no `noto3d_renderer`, no `noto3d_core`, no `glad`, no `libGL`**, which is the
  property the decision actually turns on.
- **`noto3d_asset` (GPU facade) — unchanged name, unchanged public contract.** Depends on
  `noto3d_asset_cpu` (PUBLIC) **and** `noto3d_renderer` (PUBLIC). It owns `uploadToGpu()`, the
  `AssetRegistry` class (all six existing entry points: `load` / `loadManifestOnly` / `reload` /
  `loadModelShapesOnly` / `loadModelFile` / `model`), the GPU dedup caches, and the two standalone
  escape hatches (ADR-0007 amendment). The class and every public signature stay **as-is** — this
  is a linkage refactor, not an API change.
- **The `AssetRegistry` name and every existing entry point are preserved**; the CPU half is an
  implementation detail behind the facade. `loadManifestOnly()` and `loadModelShapesOnly()` are
  *not* deleted (0025③ locked them as public production API) — they simply become thin
  CPU-only paths through the facade that do **not** call `uploadToGpu()`.

Rationale for the split over a header-only or status-quo fix is in **Rejected alternatives**
below; the deciding factor is that the split is the *only* option that lets `tests/asset/` and a
future `--list-assets` consumer stop **linking GL at all** (ticket 0039's target), which is the
concrete win the spike was opened to price.

### ③ Consequences priced

- **`AssetRegistry` becomes a facade** over `noto3d_asset_cpu` (parse/dedup/entries/diagnostics)
  and its own GPU upload/caches. `CpuObj`, `loadCpuRecord()`, and the CPU dedup path move to the
  CPU library; the registry keeps a `shared_ptr<const CpuObj>` handle per canonical path
  (the cache-key discipline is unchanged).
- **`RegistryResult` aggregation moves to the CPU library** (ticket 0037). `aggregateResults()` is a
  pure function of per-asset `LoadStatus` values and `LoadDiagnostic`s — both already CPU-side types
  in `manifest_types.hpp` — so it is GL-free and belongs there. Only the per-asset *status
  bookkeeping* stays in the facade, because in `Mode::Full` the status of a record depends on
  whether its `uploadToGpu()` call succeeded. This is what lets ③'s CLI path produce a
  `RegistryResult` without linking the GPU facade.
- **`loadManifestOnly()` is retained as a public production API** (0025③), now backed entirely by
  `noto3d_asset_cpu`; `model()` / `externalModel()` still return `nullptr` in that mode — an
  existing, tested contract that must keep holding.
- **Escape hatches (ADR-0007 amendment) are behaviourally unchanged.** `loadModelShapesOnly()`
  becomes a `noto3d_asset_cpu` function (CPU-only, no GL — it already is one today);
  `loadModelFile()` stays on the GPU facade (it uploads). The 0013 `diffuseTextureRef` path
  predicate and the 0008 absolute-path-trusted escape-hatch contract apply identically on both
  sides of the split.
- **`tests/asset/` and the `--list-assets` diagnostic path stop linking GL.** After the split they
  depend on `noto3d_asset_cpu` (and, for the CLI, the small `AssetRegistry`-free path 0037 wires);
  `ctest` stays green and `--list-assets` still never creates a GL context. **Amendment (0038) —
  the `AssetRegistry` header is GL-free and the class is split across two translation units.**
  `asset_registry.hpp` no longer includes `model.hpp`/`texture.hpp`; it re-declares
  `renderer::Model` (`using Model = std::vector<SubMesh>` over an incomplete `SubMesh`) and
  `renderer::Texture`, which is sound because the registry only holds them behind `shared_ptr` and
  hands out borrowed pointers. `asset_registry_cpu.cpp` (in `noto3d_asset_cpu`) defines
  `loadManifestOnly()`, `loadModelShapesOnly()` and every read-only accessor; `asset_registry.cpp`
  (in `noto3d_asset`) keeps `load()`, `loadModelFile()`, `reload()` and `uploadToGpu()`. The single
  mode-branching `doLoad(path, mode)` became two mode-specific drivers that share one
  `noto3d_asset_cpu` pipeline, so the CPU half cannot drift between modes.
  - **`reload()` deliberately stays in the facade.** Its contract is "repeat the last `load()` *or*
    `loadManifestOnly()`", so a CPU-only link cannot implement it without silently dropping GPU
    models on a Full reload — and ADR-0010 depends on `reload()` being able to invalidate borrowed
    `DrawItem::model` pointers after a Full load. Its two test cases therefore live in a
    GL-linked target of their own (the ticket's own escape clause), not by re-linking the CPU suite.
  - **`loadManifestCpu()` is the one implementation of the manifest CPU pipeline** (canonicalisation,
    GPU-dedup ordering, `fallback_geometry`, the 0013 predicate, the up-axis warning). It has three
    callers — `loadManifestOnly()`, `load()`'s CPU half, and `--list-assets` — which is what makes
    "the CLI output is unchanged" a structural property rather than a coincidence that holds only
    while the manifest is clean. A `parseManifest()`-only CLI would have printed the listing lines
    byte-identically while silently dropping every per-asset warning.
  - **Reading the boundary from `nm`:** the CPU library contains **0 GL/glad function symbols**,
    but `AssetRegistry`'s GPU cache members make `SubMesh`/`Texture` appear in *mangled STL
    plumbing* names (`_Hashtable`, `__shared_ptr::get`) instantiated by the implicit destructor.
    Those are references to **incomplete** types — the include-closure guard, not the symbol
    names, is the authoritative check. This is the unavoidable cost of ①b: one registry owns both
    caches, so the CPU half must be able to destroy them.
- **`noto3d_app` and the `noto3d-editor` executable are unchanged** (they already needed the GPU
  facade for `collectDrawItems` / `--demo` / the main render loop). M4's single-seam contract
  (`collectDrawItems(scene, resolver)`) is untouched.
- **The public CMake visibility of the split is a new edge:** `noto3d_asset` PUBLIC-links
  `noto3d_asset_cpu` (additive) and keeps its PUBLIC link to `noto3d_renderer`. No consumer of the
  GPU facade loses a dependency. The new *only* consumer of `noto3d_asset_cpu` is the CPU test /
  CLI path.

---

## Rejected alternatives

- **One library with a GL-free header subset (move only `Vertex`, keep `noto3d_asset` monolithic).**
  Rejected as the *final* shape, kept as the **first tracer step** (ticket 0036). It removes the
  `glad/gl.h` reach from the CPU parse header but **cannot** stop `tests/asset/` from *linking*
  `libGL`, because the target still PUBLIC-links `noto3d_renderer`. Cheap and low-risk, but it
  under-delivers on the concrete "CPU path links no GL" win; it is a stepping stone, not the
  decision.
- **Keep as-is and accept the transitive link.** Rejected: it leaves the CPU↔GPU type coupling
  (one `AssetRegistry` header exposing `Model*`/`Texture`) and the GL reach in place — exactly the
  question 0027 exists to answer — and would let M6's Save/Load design rest on an unexamined model.
- **A `renderer`-side upload entry point (upload owned by the renderer, not the asset layer).**
  Rejected: it inverts the M4 contract. ADR-0010 Q5 + ADR-0007 keep `AssetRegistry` the sole
  asset authority and keep resolution in the app seam; moving upload into the renderer would make
  the renderer depend on manifest/OBJ knowledge, coupling the two milestones the split is meant to
  separate. Direction of the split is asset_cpu → asset (facade) → renderer, never renderer → asset.
- **One CPU `Material` type vs a separate GPU `Material` (does a CPU Material become a GPU
  Material at upload, and who owns the conversion?).** Rejected the split material: it would
  duplicate the ADR-0006 field contract and add a conversion step for no isolation gain (`Material`
  is already GL-free; only its optional `texture` handle is GPU-flavoured, and the ① invariant
  keeps that null on the CPU side). **Conversion is owned by `uploadToGpu()`**, which fills
  `material.texture` and moves the `Material` into the `SubMesh`. One type, no conversion ambiguity.
- **Making `CpuObj` / `CanonicalShape` fully asset-owned (own `Vertex`/`Material` copies).**
  Rejected: it duplicates the 32 B vertex layout (ADR-0006 static_assert) in two places and risks
  layout drift. Sharing the renderer's GL-free data types is the point of ①.

## Execution (tracer bullets)

Implementation is split into small tickets, **CPU-first**, so each step is independently green.
No ticket changes observable behaviour; the full ctest suite (22/22 dev + release since 0038) plus the
`--list-assets` pre-GL path and the 0024/0033 CLI matrices must stay green throughout.

- **0036** — extract `renderer::Vertex` into a GL-free `vertex.hpp`; narrow `obj_loader.hpp` /
  `asset_registry.hpp` includes. *(the rejected "one-library, GL-free header subset" as tracer 1)*
- **0037** — create `noto3d_asset_cpu`: move `manifest_types`, `obj_loader`, `CpuObj`,
  `loadCpuRecord` + parse/dedup into it; keep `AssetRegistry` as a thin CPU-consuming facade.
- **0038** — `tests/asset` + the `--list-assets` CLI path build against `noto3d_asset_cpu`;
  prove via CMake `LINK_LIBRARIES` that these targets no longer pull `noto3d_renderer`/`glad`.
- **0039** — close-out: verify the full graph, re-run ctest dev+release + the CLI matrices, and
  confirm `noto3d-editor`/`noto3d_app` are unchanged.

**Points to ADR:** 0006 (vertex/material contract) · 0007 (registry authority + escape hatches) ·
0008 (module boundaries, declaration-only headers) · 0009 (0017 pre-GL `--list-assets` CLI) ·
0010 (Q5 borrowed `DrawItem::model`, Q7 camera purity) · 0011 (Q1 premise correction, M5 gate).
