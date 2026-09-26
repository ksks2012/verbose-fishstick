#include "scene.hpp"

// ADR-0010 Q3 locks glm::eulerAngleYXZ as the sole YXZ implementation; it lives
// in an experimental GTX header, so enable the opt-in locally to this TU only.
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

namespace noto3d::scene {

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