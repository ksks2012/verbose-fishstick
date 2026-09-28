#pragma once

#include <scene.hpp>

#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>

namespace noto3d::app {

// The Editor facade owns the session's Scene by value and is the sole write
// path for all UI code (ADR-0011 Q3/Q11).
//
// Key contracts:
//   * No mutable scene::Scene&, Entity*, or entity span is ever returned to
//     callers; UI code structurally cannot write through a retained pointer.
//   * All entity mutations are EntityId-keyed operations on this class.
//   * Each re-resolution call goes back to the id; no pointers cross frames.
//   * destroyEntity() and clear() explicitly clear a stale selection so it
//     is never silently dangling.
//   * EditorState (the selected EntityId) lives here, separate from the
//     per-view ViewportState and the per-frame InputState (ADR-0011 Q6).
class Editor {
public:
    Editor() = default;

    // === Entity lifecycle mutations =========================================
    scene::EntityId createEntity(std::string name, std::string assetId);
    bool            destroyEntity(scene::EntityId id) noexcept;
    void            clear() noexcept;  // clears all entities; resets selection

    // === Thin EntityId-keyed field mutations ================================
    bool setEntityPosition(scene::EntityId id, glm::vec3 pos)             noexcept;
    bool setEntityRotation(scene::EntityId id, glm::vec3 rotationRad)     noexcept;
    bool setEntityScale   (scene::EntityId id, glm::vec3 scale)           noexcept;
    bool setEntityTransform(scene::EntityId id, const scene::Transform& t) noexcept;
    // Reassigns the entity's assetId (M5 asset picker, ticket 0031).  The only
    // effect is AssetId mutation: two entities on the same asset stay
    // independent, and the Scene never names a Model/GPU object (ADR-0011 Q1).
    bool setEntityAssetId(scene::EntityId id, std::string assetId) noexcept;

    // === Entity enumeration (value copy for UI; no span, no pointer) ========
    struct HierarchyItem {
        scene::EntityId id;
        std::string     name;
        std::string     assetId;
    };
    [[nodiscard]] std::vector<HierarchyItem> hierarchyItems() const;
    [[nodiscard]] std::size_t                entityCount()    const noexcept;

    // === Selection (editor-session state, ADR-0011 Q6) =====================
    // select() stores the id and logs it; hasSelection() returns false if the
    // entity no longer exists (allows UI to distinguish "nothing" from "stale").
    void select(scene::EntityId id) noexcept;
    void deselect() noexcept;

    [[nodiscard]] scene::EntityId           selectedId()      const noexcept;
    [[nodiscard]] bool                       hasSelection()    const noexcept;
    [[nodiscard]] std::optional<std::string> selectedName()    const noexcept;
    [[nodiscard]] std::optional<std::string> selectedAssetId() const noexcept;
    // Returns the selected entity's transform; nullopt when nothing is selected
    // or the selection id is stale.  Re-resolves every call (no cached copy).
    [[nodiscard]] std::optional<scene::Transform> selectedTransform() const noexcept;

    // === Per-id entity data access ==========================================
    // Re-resolves each call; callers must never retain the return value.
    [[nodiscard]] std::optional<std::string> entityName(scene::EntityId id) const noexcept;

    // === Read-only scene access (rendering seam only) =======================
    // Passed to collectDrawItems(); callers must not retain the reference across
    // mutations (spans from entities() are invalidated by structural mutations).
    [[nodiscard]] const scene::Scene& scene() const noexcept;

private:
    scene::Scene    scene_;
    scene::EntityId selection_{scene::InvalidEntityId};
};

}  // namespace noto3d::app
