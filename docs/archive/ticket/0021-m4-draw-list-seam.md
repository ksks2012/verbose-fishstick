# 0021 — M4 asset-resolution draw-list seam

**What to build:**

The application-level orchestration seam that turns `scene::Scene` + `AssetRegistry` resolution
into frame-local `DrawItem`s — per ADR-0010 Q5. This ticket does NOT bind the registry into Scene;
Scene stays pure data (Q1).

```cpp
namespace noto3d::app {

struct DrawItem {
    const renderer::Model* model;   // borrowed, this-frame resolution; no ownership
    std::string            assetId; // diagnostic identity
    glm::mat4              modelMatrix; // == scene::modelMatrix(entity.transform)
};

using Resolver = std::function<const renderer::Model*(const std::string&)>;

std::vector<DrawItem> collectDrawItems(const scene::Scene& scene, const Resolver& resolve);

}
```

## Scope

**A — draw-list collection (`src/app/`):**
- Iterate `scene.entities()` in creation order; per entity:
  `resolve(entity.assetId)` → null ⇒ **skip (no DrawItem), not failure** (Q1/Q5); else produce one
  `DrawItem` with `modelMatrix = scene::modelMatrix(entity.transform)`.
- **`DrawItem::model` lifetime (Q5)**: the pointer is **borrowed** — valid only within the frame
  in which resolution was performed. Each frame must re-obtain a fresh draw list; a `DrawItem`
  must never be retained across `AssetRegistry` mutation/reload (the same invalidation discipline
  Q2 imposes on Scene references). 0021 documents this in the header; no code takes ownership.
- **`Resolver` must be callable (Q5)**: an empty `std::function` is a **programming error**, never
  a supported runtime state. `collectDrawItems` checks `resolve` at entry and **throws
  `std::logic_error`** (in every build; never `std::bad_function_call`, never a silent empty draw
  list). An `assert` may additionally guard debug builds.
- Two Entities sharing an `assetId` ⇒ **two DrawItems** (no deduplication) — asset identity ≠
  entity identity (Q5).
- No class, no resolution cache, no state stored back into Scene/Entity; `DrawItem` is
  frame-local data (Q5).

**B — headless tests (`tests/app/`, new dir):**
- Resolved entity ⇒ 1 DrawItem with expected `model` + `assetId`.
- Unresolved entity ⇒ skipped, remaining entities still emitted (scene itself is never invalidated).
- N entities ⇒ N DrawItems (incl. duplicate assetId case).
- `modelMatrix` equals `scene::modelMatrix(entity.transform)` for a non-trivial transform (proves
  0020's single matrix definition is actually consumed here).
- Empty resolver ⇒ `std::logic_error` thrown (headless test asserts the throw); never a silent
  empty list.
- Resolver is injectable (fake registry) — no GL/GPU required.

## Acceptance

- [x] `collectDrawItems()` returns per-entity DrawItems; unresolved assetId ⇒ skip, not failure
- [x] No deduplication/class/cache; DrawItem holds no ownership and is never stored back
- [x] `DrawItem::model` documented as a frame-local borrowed pointer (invalidated by registry
      reload; fresh draw list each frame)
- [x] Empty `Resolver` ⇒ `std::logic_error` (headless-tested), never a silent empty draw list
- [x] `modelMatrix == scene::modelMatrix(entity.transform)` verified headlessly
- [x] `tests/app/` suite green (dev + release); fake resolver, no GPU
- [x] `scene` module untouched (no new dependency added to it)

**Blocked by:** 0020 (complete)
**Status:** completed

> Evidence (2026-09-27): `src/app/draw_list.{hpp,cpp}` land `collectDrawItems()` — deterministic
> creation-order iteration; unresolved assetId → skip (never failure; Scene unmodified); no
> dedup/cache/class; `DrawItem::model` documented as a frame-local borrowed pointer (fresh draw
> list each frame, never retained across registry mutation/reload). Empty `Resolver` ⇒
> `std::logic_error` thrown at entry (never `std::bad_function_call`, never a silent empty list),
> plus an `assert` guard in debug. `tests/app/draw_list_test.cpp` = 7 blocks / 15 checks: resolved
> entity → 1 DrawItem (model + assetId), unresolved → skipped while remaining entities still emit,
> N entities → N DrawItems incl. duplicate assetId (two items, no dedup), `modelMatrix` equality
> against 0020's `scene::modelMatrix` on a non-trivial transform, empty-resolver throw, empty
> scene → empty list. New `noto3d_app` STATIC lib + `noto3d-app-tests` + CTest `app-draw-list-seam`.
> ctest 7/7 on dev + release (suite count 6→7); test binary prints `ALL PASS`. `scene` module
> untouched.
**Points to ADR:** 0010 (Q1/Q5) · 0007 (AssetRegistry is the sole asset authority)
**Rolls under spec:** ADR-0010 (M4 spec embedded; execution tickets only in docs/ticket/)