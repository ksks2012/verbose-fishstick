#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// M4 Scene — pure data layer (ADR-0010 Q1–Q4).
//
// Scene/Entity store stable asset IDs and world-space transform data ONLY.
// They never reference AssetRegistry, Model, GPU objects, renderer state, or a
// logger. Runtime asset resolution happens in the app orchestration layer
// (app::collectDrawItems, ticket 0021).
//
// Identity contract (Q2/Q4):
//   * EntityId is uint32_t; 0 = InvalidEntityId; first generated id = 1.
//   * Ids are monotonic and never reused for the lifetime of the Scene object;
//     clear() removes entities but does NOT reset the counter.
//   * EntityId is an identity, never a storage index (id != vector position).
//   * entities() iterates in creation order; lookup is a linear scan.
//   * References/pointers/spans obtained from Scene are invalidated by
//     subsequent structural mutation (create/destroy/clear).
//
// M6 forward contract (Q4): a Scene JSON file is a complete snapshot; loading
// replaces the whole Scene including the id allocation state, then sets
// nextEntityId = max(restored ids) + 1.
//
// M6 restore entry (ADR-0013 Q1, ticket 0042): fromEntities() is the ONLY way
// to build a Scene that carries pre-existing ids.  It exists because
// nextEntityId_ is private with no setter, so before this there was no way to
// inject a restored scene at all.  The allocator is *derived* from the incoming
// ids rather than accepted as an argument, which is what makes "the counter is a
// pure function of the entity set" a type-level property: there is no overload
// to pass an arbitrary value to.
namespace noto3d::scene {

using EntityId = std::uint32_t;

inline constexpr EntityId InvalidEntityId = 0;

struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};  // Euler radians, YXZ order: x=pitch, y=yaw, z=roll
    glm::vec3 scale{1.0f};
};

struct Entity {
    EntityId    id{InvalidEntityId};
    std::string name;
    std::string assetId;
    Transform   transform;
};

// Pure math, sole transform->matrix definition (Q3): radians, YXZ via
// glm::eulerAngleYXZ, M = T · R · S. Render orchestration composes
// projection * view * modelMatrix(entity.transform).
glm::mat4 modelMatrix(const Transform& transform);

class Scene {
public:
    // Monotonic id starting at 1; never reused (clear() included).
    EntityId createEntity(std::string name, std::string assetId);

    // M6 restore entry (ADR-0013 Q1).  Builds a Scene from entities that
    // already carry their own ids, preserving the given order (which becomes
    // creation order), and DERIVES the allocator as max(ids) + 1 — or 1 for an
    // empty set.  There is deliberately no setter, no (entities, nextId)
    // overload, and no in-place variant: the counter can only ever arrive as a
    // consequence of the entity set.
    //
    // PRECONDITION (not validation): every id is non-zero and ids are unique.
    // A violation is a programming error, checked by a debug-only assert and
    // undefined in release — it is NOT a diagnostic path.  Structural
    // validation of an untrusted *document* (non-v1, bad shape, id == 0,
    // duplicate id) is the serializer's job: ADR-0013 Q3, ticket 0044.  Keeping
    // that in one place is deliberate; a graceful return here would create a
    // second, half-validating half of the same rule.
    static Scene fromEntities(std::vector<Entity> entities);

    // Erase-preserving-creation-order; returns false (no-op) for unknown ids.
    bool destroyEntity(EntityId id) noexcept;

    // Remove all entities without resetting the id counter (Q4).
    void clear() noexcept;

    Entity*       findEntity(EntityId id) noexcept;
    const Entity* findEntity(EntityId id) const noexcept;

    std::span<Entity>       entities() noexcept;
    std::span<const Entity> entities() const noexcept;

private:
    std::vector<Entity> entities_;
    EntityId            nextEntityId_{1};
};

}  // namespace noto3d::scene