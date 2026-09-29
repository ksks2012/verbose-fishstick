#include "scene.hpp"

#include <algorithm>
#include <cassert>

// ADR-0010 Q3 locks glm::eulerAngleYXZ as the sole YXZ implementation; it lives
// in an experimental GTX header, so enable the opt-in locally to this TU only.
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

namespace noto3d::scene {

Scene Scene::fromEntities(std::vector<Entity> entities) {
    // Precondition check, debug only.  A restored set must carry non-zero,
    // unique ids; the derived allocator below silently produces a broken Scene
    // otherwise (findEntity would resolve the first match and ids could repeat
    // within one lifetime).  This is a contract marker, not a validation path —
    // see the header note and ADR-0013 Q3: the serializer (ticket 0044) is the
    // single place that rejects a malformed *document*.
    for (std::size_t i = 0; i < entities.size(); ++i) {
        assert(entities[i].id != InvalidEntityId &&
               "Scene::fromEntities: a restored entity id must be non-zero");
        for (std::size_t j = i + 1; j < entities.size(); ++j) {
            assert(entities[i].id != entities[j].id &&
                   "Scene::fromEntities: restored entity ids must be unique");
        }
    }

    // Derived, never accepted (ADR-0013 Q1): the allocator is a pure function of
    // the incoming id set.  maxId stays InvalidEntityId (0) for an empty set,
    // which is exactly the "first id is 1" case.
    EntityId maxId = InvalidEntityId;
    for (const auto& e : entities) {
        maxId = std::max(maxId, e.id);
    }

    Scene scene;
    scene.entities_    = std::move(entities);
    scene.nextEntityId_ = (maxId == InvalidEntityId) ? EntityId{1} : maxId + EntityId{1};
    return scene;
}

EntityId Scene::createEntity(std::string name, std::string assetId) {
    Entity e;
    e.id      = nextEntityId_++;
    e.name    = std::move(name);
    e.assetId = std::move(assetId);
    entities_.push_back(std::move(e));
    return entities_.back().id;
}

bool Scene::destroyEntity(EntityId id) noexcept {
    for (auto it = entities_.begin(); it != entities_.end(); ++it) {
        if (it->id == id) {
            entities_.erase(it);
            return true;
        }
    }
    return false;
}

void Scene::clear() noexcept {
    entities_.clear();
    // nextEntityId_ intentionally NOT reset (ADR-0010 Q4).
}

Entity* Scene::findEntity(EntityId id) noexcept {
    for (auto& e : entities_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

const Entity* Scene::findEntity(EntityId id) const noexcept {
    for (const auto& e : entities_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

std::span<Entity> Scene::entities() noexcept {
    return entities_;
}

std::span<const Entity> Scene::entities() const noexcept {
    return entities_;
}

glm::mat4 modelMatrix(const Transform& transform) {
    const auto t = glm::translate(glm::mat4(1.0f), transform.position);
    // YXZ order — yaw/pitch/roll map to rotation.y/x/z (ADR-0010 Q3).
    const auto r = glm::eulerAngleYXZ(transform.rotation.y,
                                      transform.rotation.x,
                                      transform.rotation.z);
    const auto s = glm::scale(glm::mat4(1.0f), transform.scale);
    return t * (r * s);  // M = T · R · S
}

}  // namespace noto3d::scene