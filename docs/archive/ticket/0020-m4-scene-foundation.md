# 0020 — M4 Scene foundation

**What to build:**

The pure-data M4 Scene: `EntityId`, `Transform`, `Entity`, `Scene`, and the single transform-matrix
function `scene::modelMatrix()`. This is the first M4 ticket and the architecture the rest of the
milestone hangs on — per ADR-0010 decisions Q1–Q4.

Scene must be **pure data**: no `AssetRegistry`, no `Model`, no GPU/renderer knowledge, no logging.
It only stores entities and their identity/transform data; runtime asset resolution happens in the
app layer (0021).

Roles: 0020 = Scene foundation · 0021 = draw-list/resolution seam · 0022 = multi-entity runtime
rendering · 0023 = camera & interaction · 0024 = integration/regression.

## Scope

**A — types (`src/scene/scene.hpp`):**
```cpp
namespace noto3d::scene {
using EntityId = std::uint32_t;
inline constexpr EntityId InvalidEntityId = 0;

struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};  // Euler radians: x=pitch, y=yaw, z=roll (YXZ)
    glm::vec3 scale{1.0f};
};

struct Entity {
    EntityId    id{InvalidEntityId};
    std::string name;
    std::string assetId;
    Transform   transform;
};

class Scene { ... };
glm::mat4 modelMatrix(const Transform& transform);  // T·R·S, glm::eulerAngleYXZ
}
```

**B — `Scene` API (Q2/Q4 contract):**
- `createEntity(std::string name, std::string assetId) -> EntityId` — monotonic, starts at 1,
  never reused; id ≠ vector index.
- `destroyEntity(EntityId) -> bool` — erase, preserves creation order; unknown id → `false`, no-op.
- `findEntity` const/non-const — linear scan, O(n) editor scale.
- `entities()`/`entities() const` — `std::span`, creation order.
- `clear()` — removes all entities **without resetting the id counter** (Q4).
- Documented lifetime contract: pointers/spans invalidated by structural mutation.

**C — `modelMatrix(const Transform&)` (Q3):** radians, YXZ via `glm::eulerAngleYXZ`, `M = T·R·S`
(scale → rotate → translate). Sole definition; 0022/0024 consume it, never recompose.

**D — headless tests (`tests/scene/scene_test.cpp`):**
- ID uniqueness / monotonic / never reused across destroy / `EntityId 0 = Invalid`.
- ID ≠ storage index; unknown-id destroy → `false` no-op; creation-order preserved after erase.
- `clear()` empties but counter keeps climbing.
- `modelMatrix`: identity transform → identity; translation moves origin; scale per-axis;
  non-symmetric rotation verified mathematically under the project convention; composition fixture
  asserts `M == T·R·S` exactly (guards against future `T·S·R`).
- M6 forward contract is documentation only in 0020 — no serialization code.

## Acceptance

- [x] `Scene`/`Entity`/`Transform`/`EntityId` land with no asset/renderer/GL dependency
- [x] `EntityId`: monotonic from 1, `0 = Invalid`, never reused (`clear()` included)
- [x] `destroyEntity` returns bool; creation order preserved; unknown id → `false`
- [x] `modelMatrix()` == identity for default transform; T·R·S order proven by a composition test
- [x] Headless `tests/scene/` suite green (dev + release), no GPU
- [x] No serialization, no registry, no renderer/GL surface introduced

**Blocked by:** M3 closed (0018 archived)
**Status:** completed

> Evidence (2026-09-26): `src/scene/scene.{hpp,cpp}` land `EntityId/Transform/Entity/Scene/
> modelMatrix` — pure data, glm only (`noto3d_scene`, no asset/renderer link). `tests/scene/
> scene_test.cpp` = 28 checks: identity/storage invariants (monotonic ids, id≠index, creation
> order preserved after erase, unknown-id destroy → `false`, ids never reused after `clear()`),
> defaults, and matrix math (identity / translation / per-axis scale / yaw+π2: +X→−Z, pitch+π2:
> +Y→+Z under project Y-up convention / non-commuting T·R·S fixture with an independently
> assembled expected matrix using the locked T·R·S order + element-wise equality against the
> production path). Note: `glm::eulerAngleYXZ` is an experimental GTX
> API (`GLM_ENABLE_EXPERIMENTAL` opt-in), kept local to the two TUs that use it. ctest 6/6 on
> dev + release (suite count 5→6, new `scene-entity-contract`).
**Points to ADR:** 0008 (module layout) · 0010 (Q1–Q4)
**Rolls under spec:** ADR-0010 (M4 spec embedded; execution tickets only in docs/ticket/)