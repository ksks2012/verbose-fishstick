# 0027 — Asset CPU/GPU boundary architecture spike

**Source:** code review of 0021–0024 (items 003/005), split out of 0025 in the Sep 2026 re-review.
**Status:** **complete** — spike closed 2026-09-29 in **ADR-0012**; execution split into
**0036–0039** (CPU-first tracer bullets)
**Blocked by:** none — the ownership model is now determined, so M6's snapshot persistence may
proceed; the *implementation* (0036–0039) is code-quality work that does not block M6 scope

**Premise correction (ADR-0011 Q1, 2026-09-28).** This ticket was written on the premise that
"AssetRegistry's CPU-side manifest/OBJ parsing is fused to a GL upload path". The 2026-09-28
review found that premise out of date — **a CPU/GPU boundary already exists and is already
enforced**:

- `src/asset/` makes **no direct GL calls**.
- `loadObjShapes()`, `CanonicalShape` and `loadModelShapesOnly()` are already **CPU-only**.
- `uploadToGpu()` is already the single, explicit GPU-upload gate.

What actually remains is **header/type coupling**, not a missing runtime boundary:
`AssetRegistry`'s public API exposes `renderer::Model*` and caches a `Texture`, so the CPU-side
question ("which assets exist?") and the GPU-side question ("what `Model` is this?") are answered
by one type. A library split would separate the *link* and the *ownership*, and that is a real
question — but it is not a defect, it is not urgent, and it is **not M5 work**.

**M5 binding invariant (ADR-0011 Q1):** M5 introduces **no new ownership** of `Model`, `Mesh`,
`Texture`, or any GPU resource, and does not resolve this coupling. Asset *editing* in M5 means
mutating a stable `assetId` string and nothing else. If an M5 ticket needs a `Model*`, a
`Texture`, or a GPU-resource lifetime, that is a signal to bring this ticket forward — not to
solve it inline.

**Output of the spike (unchanged):** architecture investigation → proposed boundary → ADR (or ADR
amendment) → implementation tickets (tracer-bullet). The ADR carries the type-ownership decision
and a rejected-alternatives section; this ticket itself decides nothing.

**Output of the spike:** architecture investigation → proposed boundary → ADR (or ADR amendment)
→ implementation tickets (tracer-bullet). The ADR carries the type-ownership decision and a
rejected-alternatives section; this ticket itself decides nothing.

## Existing M4 contracts to preserve unless explicitly superseded

A boundary refactor must not silently undo what M4 just locked (ADR-0010 Q1/Q5/Q6, ADR-0007,
ADR-0009). These hold unless a new ADR says otherwise:

- `AssetRegistry` remains the **sole asset authority** (no parallel loader, no implicit fallback).
- `Scene` stores **only stable `assetId` strings** — no `Model` pointers, no GL objects; Scene
  stays pure data.
- `app::collectDrawItems(scene, resolver)` remains the **app-layer resolution seam**; resolution
  stays out of Scene.
- `DrawItem::model` is a **borrowed, frame-local `const renderer::Model*`** — it owns nothing and
  is never cached across frames or registry reloads.
- Standalone escape hatches (`loadModelShapesOnly()`, `loadModelFile()`, ADR-0007 amendment) keep
  their documented behaviour and the path-predicate safety surface.
- `--asset` / `--demo` / default entry modes, their titles and `M4_INFO … entities=<N>` logging stay
  observationally identical.

---

## ① Decide type ownership

Which types belong to the asset (CPU) domain vs the renderer (GPU) domain?

```
AssetRecord / CanonicalShape / manifest representation / CPU-side Material
        vs
Mesh / SubMesh / GPU Material / Texture / Model
```

Open questions: where does `Model` (a `vector<SubMesh>` of `renderer::Mesh`) sit? Does a CPU
`Material` become a GPU `Material` at upload, and who owns the conversion?

## ①b Decide ownership and lifetime of CPU data and GPU resources

Splitting the library also raises a lifetime question, not just an include question:

- After upload, who owns the CPU asset data and the GPU resource — does `AssetRegistry` hold both a
  CPU `AssetRecord` **and** a GPU `Model`, or does a `Renderer`/`AssetUploader` own GPU objects with
  the registry holding CPU data only?
- What invalidates a `Model*` returned through the `Resolver` (per the M4 borrowed-pointer
  contract), and does the answer survive the split?
- Does the GL-owning-lifetime discipline (objects destroyed while the context is current, 0012) get
  narrower, wider, or unchanged?

This interacts directly with the preserved contracts above; answer it before choosing a library
shape.

## ② Decide the library shape

Candidate: `noto3d_asset_cpu` (no GL) + `noto3d_asset_gpu` (depends on both, performs upload).
Evaluate alternatives (one library with a GL-free header subset; keep as-is and accept the
transitive link; a `renderer`-side upload entry point instead of an asset-side one).

## ③ Consequences to price

- `AssetRegistry` becomes two-phase (parse → upload) or a facade over both.
- `loadManifestOnly()` (0025③) either becomes the *legitimate* CPU-only entry point of
  `asset_cpu`, or is replaced by a CPU parse function.
- 0014's `loadModelShapesOnly()` / `loadModelFile()` escape hatches (ADR-0007 amendment) must keep
  their documented behaviour across the split.
- The 0017 `--list-assets` diagnostic path and `tests/asset/` targets should end up linking **no**
  GL after the split; 0025⑧ records today's graph as the baseline.

---

## Acceptance

- [x] Spike written up as an ADR (or ADR amendment) with the type-ownership decision and the chosen
      library shape, including a rejected-alternatives section
- [x] Execution split into small implementation tickets (tracer-bullet: CPU-only manifest parse
      behind a stable entry point first)
- [x] No M4 code changes made under this ticket; 0025 stays verification-only for ③/⑤

**Points to ADR:** 0007 (AssetRegistry authority, standalone load escape hatches) · 0008 (module
boundaries) · 0009 (0017 pre-GL diagnostic CLI)

---

## Outcome — decided in ADR-0012 (2026-09-29)

This ticket is an **architecture spike**: it decides the ownership model and closes the M6 gate,
but writes no production code. The decision and its reasoning live in
**`docs/adr/0012-asset-cpu-gpu-boundary.md`**; this section records what the spike concluded and
where the work went.

**Findings (from reading the code, not from the original premise).** The runtime CPU/GPU boundary
was **already there** and enforced — `src/asset/` makes no direct GL call, `loadObjShapes()` /
`CanonicalShape` / `loadModelShapesOnly()` / `loadManifestOnly()` are CPU-only, and
`uploadToGpu()` is already the single upload gate. What remained was three kinds of coupling:
`obj_loader.hpp` (CPU) reached `glad/gl.h` **only** via `#include <mesh.hpp>` to name
`renderer::Vertex`; `AssetRegistry`'s single public header exposes GPU types (`Model*`,
`shared_ptr<Texture>`) answering both the CPU and the GPU question; and `noto3d_asset` links
`noto3d_renderer` **PUBLIC**, so every consumer — including the CPU-only asset tests and the
`--list-assets` path — transitively linked GL. All `tests/asset` cases already run CPU-only
(`model()==nullptr` / `externalModel()==nullptr`), so that link was pure waste.

**Decisions (ADR-0012).**

- **① Type ownership** — `Vertex` and `Material` are the *interchange format*, not GPU resources:
  `Vertex` moves to a new GL-free `renderer/vertex.hpp`, `Material` stays GL-free. Same namespace,
  no rename, no alias. `Mesh` / `SubMesh` / `Model` / `Texture` stay GPU-owned; `Model` is not
  moved. The CPU record carries a `Material` whose `texture` is **always `nullopt`** — the
  invariant is stronger than "usually null", and `uploadToGpu()` is its only writer.
- **①b Lifetime** — unchanged and now explicit: `AssetRegistry` remains the **sole authority and
  sole owner** of both the CPU dedup cache and the GPU caches (a facade over two libraries, not two
  authorities); `Model*` through the resolver stays the ADR-0010 Q5 **borrowed frame-local**
  pointer, invalidated by `reload()`; the 0012 GL-owning-lifetime discipline is untouched — the
  split relocates no GL owner.
- **② Library shape** — a **real split**: new `noto3d_asset_cpu` (json + tinyobjloader only; no
  renderer, no glad, no libGL) holding the parse/dedup/manifest core, and `noto3d_asset` (unchanged
  name and public contract) depending on it, still owning `uploadToGpu()`, the GPU caches, and all
  six `AssetRegistry` entry points.
- **③ Consequences** — `AssetRegistry` becomes a facade; `loadManifestOnly()` /
  `loadModelShapesOnly()` are **kept** (0025③ locked them as public production API) as thin
  CPU-only paths; the escape hatches keep their behaviour and the 0013 path predicate; and
  `tests/asset` + `--list-assets` **stop linking GL** — the concrete win the spike was opened to
  price.

**Rejected** (recorded with reasons in the ADR): single library with a GL-free header subset (kept
as tracer 1, 0036 — it cannot stop the *link* to GL); keep-as-is; a renderer-side upload entry
point (inverts the M4 contract); splitting the CPU/GPU `Material` (duplicates the ADR-0006 field
contract; one type, conversion owned by `uploadToGpu()`); asset-owned copies of `Vertex`/`Material`
(duplicates the 32 B layout, risks drift).

**Execution** — four CPU-first tracer tickets, each independently green, no observable behaviour
change: **0036** extract `renderer/vertex.hpp` · **0037** create `noto3d_asset_cpu` ·
**0038** retarget `tests/asset` + `--list-assets` and prove the GL-free link ·
**0039** close-out: final graph recorded + full regression re-run + CONTEXT/CHANGELOG update.

**Gate status.** 0027 is **closed**. M6's snapshot-persistence work may proceed: the ownership
model it depended on is now determined by ADR-0012. The 0036–0039 implementation is code-quality
work and does **not** block M6 scope.
