# Execution Summary: 0027-asset-cpu-gpu-boundary (spike)

- **Date**: 2026-09-29
- **Spec/ADR Path**: `docs/adr/0012-asset-cpu-gpu-boundary.md` (new; the spike's decision record)
- **Ticket Path**: `docs/archive/ticket/0027-asset-cpu-gpu-boundary.md` (archived on close)
- **Branch**: `main`

---

## 📌 Overview
- **Commits**: 0 new (working tree, uncommitted per implement skill `/commit` disabled)
- **Production code changed**: **none** — this is an architecture spike. ADR-0012 supersedes no
  prior decision; 0006/0007/0008/0009/0010/0011 all stand unchanged.
- **Files changed**: 6 new (ADR-0012 + tickets 0036–0039; ticket 0027 moved to archive), 2 edited
  (CONTEXT + CHANGELOG), 1 moved (0027 → `docs/archive/ticket/`).

## 🔍 What the spike found (premise correction, verified against the code)
The ticket's original premise ("manifest/OBJ parsing is fused to a GL upload path") is **out of
date** — the runtime boundary already existed and was enforced:
- `src/asset/` makes **no direct GL call**.
- `loadObjShapes()` / `CanonicalShape` / `loadModelShapesOnly()` / `loadManifestOnly()` are
  **CPU-only** (parse + dedup).
- `uploadToGpu()` is already the single GPU-upload gate.

What actually remained — read out of the headers and the link graph, not the ticket's wording —
is **header/type and link coupling**:
1. `obj_loader.hpp` (CPU) reached `glad/gl.h` **only** via `#include <mesh.hpp>` to name
   `renderer::Vertex`; `material.hpp` is already GL-free (glm only).
2. `AssetRegistry`'s single public header exposes GPU types (`loadModelFile`/`model` return
   `const renderer::Model*`; it caches `shared_ptr<renderer::Model>`/`shared_ptr<renderer::Texture>`),
   so one type answers both the CPU question and the GPU question.
3. `noto3d_asset` PUBLIC-links `noto3d_renderer` → glad, so **every** consumer transitively links
   GL — including `tests/asset/registry_test.cpp` (every case already CPU-only, asserting
   `model()==nullptr`/`externalModel()==nullptr`) and the pre-GL `--list-assets` path. That link
   was pure waste; 0025⑧ recorded it as "expected today (0027)".

## 🏛️ Decision (ADR-0012)
- **① Type ownership** — `Vertex` (32 B, ADR-0006) and `Material` are the **interchange format**,
  not GPU resources: `Vertex` moves to a new GL-free `renderer/vertex.hpp`, `Material` stays GL-free;
  same namespace, no rename/alias. `Mesh`/`SubMesh`/`Model`/`Texture` stay GPU-owned (`Model` not
  moved). The CPU record's `Material::texture` is **always `nullopt`**; `uploadToGpu()` is its only
  writer (conversion owned by upload, not by a second `Material` type).
- **①b Lifetime (unchanged, now explicit)** — `AssetRegistry` remains the **sole authority and sole
  owner** of both the CPU dedup cache and the GPU caches (a facade over two libraries, not two
  authorities — ADR-0007 verbatim). `DrawItem::model` stays the ADR-0010 Q5 borrowed frame-local
  `const renderer::Model*`, invalidated by `reload()`. The 0012 GL-owning-lifetime rule is untouched —
  the split relocates no GL owner.
- **② Library shape** — a **real split**: new `noto3d_asset_cpu` (json+tinyobjloader only; no
  renderer/glad/libGL) holds the parse/dedup/manifest core; `noto3d_asset` (unchanged name + public
  contract) depends on it, keeps `uploadToGpu()`, the GPU caches, and all six `AssetRegistry` entry
  points.
- **③ Consequences priced** — `loadManifestOnly()`/`loadModelShapesOnly()` **kept** (0025③ locked
  them as public production API) as thin CPU-only paths; escape hatches + 0013 path predicate
  unchanged; `noto3d_app`/`noto3d-editor` unchanged; **`tests/asset` + `--list-assets` stop linking
  GL** — the concrete win.
- **Rejected** (reasons in the ADR): single-library GL-free header subset (kept as tracer 1 — cannot
  stop the *link*); keep-as-is; a renderer-side upload entry (inverts the M4 contract); splitting
  CPU/GPU `Material` (duplicates ADR-0006); asset-owned `Vertex`/`Material` copies (32 B drift risk).

## 🎯 Execution tickets (CPU-first, each independently green, behaviour-neutral)
- **0036** extract `renderer/vertex.hpp` (GL-free `Vertex`); narrow `obj_loader.hpp`/`asset_registry.hpp`.
- **0037** create `noto3d_asset_cpu`; move `CpuObj`/`loadCpuRecord`/parse/dedup; keep `AssetRegistry`
  as a thin CPU-consuming facade with an unchanged public API.
- **0038** retarget `tests/asset` + `--list-assets` onto `noto3d_asset_cpu`; prove the GL-free link.
- **0039** close-out: record the final CMake graph, re-run ctest dev+release (21/21) + the CLI /
  m5-close matrices + xprop titles, update CONTEXT + CHANGELOG.

None of 0036–0039 block M6 scope; they are code-quality work on a determined ownership model.

## ✅ Verification for this ticket
The spike itself is documentation, so verification is the **consistency of the decision with the
code as it stands**, confirmed by reading: no GL calls in `src/asset/`; `Vertex` is a pure-data
struct reachable only through `mesh.hpp`'s `glad/gl.h`; `Material` is GL-free with a single
optional GL `texture` name; the CPU test cases already run CPU-only; the only GPU-side callers of
the registry are `main.cpp::load` and `editor_entry.cpp::loadModelFile`. No code was compiled
because no code changed.
