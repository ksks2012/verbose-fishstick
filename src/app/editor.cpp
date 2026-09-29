#include "editor.hpp"
#include <log.hpp>

namespace noto3d::app {

// ---------------------------------------------------------------------------
// Entity lifecycle
// ---------------------------------------------------------------------------

scene::EntityId Editor::createEntity(std::string name, std::string assetId) {
    return scene_.createEntity(std::move(name), std::move(assetId));
}

bool Editor::destroyEntity(scene::EntityId id) noexcept {
    // Explicitly clear selection if the destroyed entity was selected so the
    // id is never left as a silent dangling reference (ADR-0011 Q3).
    if (selection_ == id) {
        selection_ = scene::InvalidEntityId;
    }
    return scene_.destroyEntity(id);
}

void Editor::clear() noexcept {
    selection_ = scene::InvalidEntityId;
    scene_.clear();
}

// ---------------------------------------------------------------------------
// Field mutations
// ---------------------------------------------------------------------------

bool Editor::setEntityPosition(scene::EntityId id, glm::vec3 pos) noexcept {
    if (auto* e = scene_.findEntity(id)) {
        e->transform.position = pos;
        return true;
    }
    return false;
}

bool Editor::setEntityRotation(scene::EntityId id, glm::vec3 rotationRad) noexcept {
    if (auto* e = scene_.findEntity(id)) {
        e->transform.rotation = rotationRad;
        return true;
    }
    return false;
}

bool Editor::setEntityScale(scene::EntityId id, glm::vec3 scale) noexcept {
    if (auto* e = scene_.findEntity(id)) {
        e->transform.scale = scale;
        return true;
    }
    return false;
}

bool Editor::setEntityTransform(scene::EntityId id, const scene::Transform& t) noexcept {
    if (auto* e = scene_.findEntity(id)) {
        e->transform = t;
        return true;
    }
    return false;
}

bool Editor::setEntityAssetId(scene::EntityId id, std::string assetId) noexcept {
    // Effect on the Scene is LIMITED to the entity's assetId (ADR-0011 Q1/Q7):
    // no Model/Mesh/Texture/GPU-resource ownership is introduced here; runtime
    // resolution of the new id stays in the collectDrawItems seam (ADR-0010 Q5).
    if (auto* e = scene_.findEntity(id)) {
        e->assetId = std::move(assetId);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Entity enumeration
// ---------------------------------------------------------------------------

std::vector<Editor::HierarchyItem> Editor::hierarchyItems() const {
    const auto entities = scene_.entities();  // span<const Entity>
    std::vector<HierarchyItem> items;
    items.reserve(entities.size());
    for (const auto& e : entities) {
        items.push_back({e.id, e.name, e.assetId});
    }
    return items;
}

std::size_t Editor::entityCount() const noexcept {
    return scene_.entities().size();
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

void Editor::select(scene::EntityId id) noexcept {
    selection_ = id;
    // Log on a valid re-resolve so the selection is observable in the
    // structured log (M5_SEL format, consistent with M0_OK/M4_INFO pattern).
    if (id != scene::InvalidEntityId) {
        if (const auto* e = scene_.findEntity(id)) {
            NOTO3D_INFO("M5_SEL  entity='{}' id={}", e->name, static_cast<unsigned>(id));
        }
    }
}

void Editor::deselect() noexcept {
    selection_ = scene::InvalidEntityId;
}

scene::EntityId Editor::selectedId() const noexcept {
    return selection_;
}

bool Editor::hasSelection() const noexcept {
    return selection_ != scene::InvalidEntityId &&
           scene_.findEntity(selection_) != nullptr;
}

std::optional<std::string> Editor::selectedName() const noexcept {
    if (const auto* e = scene_.findEntity(selection_)) {
        return e->name;
    }
    return std::nullopt;
}

std::optional<std::string> Editor::selectedAssetId() const noexcept {
    if (const auto* e = scene_.findEntity(selection_)) {
        return e->assetId;
    }
    return std::nullopt;
}

std::optional<scene::Transform> Editor::selectedTransform() const noexcept {
    if (const auto* e = scene_.findEntity(selection_)) {
        return e->transform;
    }
    return std::nullopt;
}

std::optional<std::string> Editor::entityName(scene::EntityId id) const noexcept {
    if (const auto* e = scene_.findEntity(id)) {
        return e->name;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Scene access (rendering seam)
// ---------------------------------------------------------------------------

const scene::Scene& Editor::scene() const noexcept {
    return scene_;
}

// ---------------------------------------------------------------------------
// Scene replacement (M6 load injection)
// ---------------------------------------------------------------------------

void Editor::replaceScene(scene::Scene scene) noexcept {
    scene_ = std::move(scene);
    // Selection is session state, never persisted (ADR-0013 Q2).  Clear it via
    // the existing deselect() so there is exactly one place that writes
    // selection_ to InvalidEntityId for "nothing is selected".
    deselect();
}

}  // namespace noto3d::app
