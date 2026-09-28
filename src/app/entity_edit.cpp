#include "entity_edit.hpp"
#include <log.hpp>

namespace noto3d::app {

scene::EntityId commitCreateEntity(Editor& editor, const AssetChoice& choice) {
    // The entity's name defaults to its asset id (ticket 0032; Scene names need
    // not be unique).  The assetId is the choice's id, so the entity is drawable
    // by construction and never starts life unresolvable (ADR-0011 Q7).
    const auto id = editor.createEntity(choice.id, choice.id);
    NOTO3D_INFO("M5_ENTITY created entity='{}' id={} assetId='{}' count={}",
                choice.id, static_cast<unsigned>(id), choice.id,
                editor.entityCount());
    return id;
}

void commitDestroySelected(Editor& editor) {
    if (!editor.hasSelection()) {
        return;  // silent no-op (ticket 0032): destroy with nothing selected
    }
    const auto id         = editor.selectedId();
    const auto storedName = editor.selectedName();
    editor.destroyEntity(id);
    NOTO3D_INFO("M5_ENTITY destroyed entity='{}' id={} count={}",
                storedName ? *storedName : "???", static_cast<unsigned>(id),
                editor.entityCount());
}

}  // namespace noto3d::app