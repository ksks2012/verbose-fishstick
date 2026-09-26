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