#include "asset_picker.hpp"

#include <log.hpp>

namespace noto3d::app {

// ---------------------------------------------------------------------------
// Snapshot construction (ADR-0011 Q7)
// ---------------------------------------------------------------------------

AssetPickerSnapshot buildAssetPickerSnapshot(const asset::AssetRegistry& registry) {
    AssetPickerSnapshot snap;

    for (const auto& entry : registry.entries()) {
        // Renderable = geometry present.  The ADR-0007 invariant is
        // "geometrySource non-null iff geometry non-null", so checking the
        // typed source is exactly checking drawability: reference-only and
        // geometry-less entries both have no geometry source and are hidden.
        if (entry.geometrySource.has_value()) {
            snap.choices.push_back(AssetChoice{entry.id, *entry.geometrySource});
        } else {
            ++snap.hiddenCount;
        }
    }
    snap.renderableCount = snap.choices.size();

    // Hiding is observable (Q7/Q8): report the counts even when zero so the
    // log line is present both with and without renderable assets.
    NOTO3D_INFO("M5_ASSET asset-picker n={} hidden={}",
                static_cast<int>(snap.renderableCount),
                static_cast<int>(snap.hiddenCount));

    return snap;
}

// ---------------------------------------------------------------------------
// Commit (ADR-0011 Q3/Q11)
// ---------------------------------------------------------------------------

void commitAssetAssignment(Editor& editor, scene::EntityId id, std::string assetId) {
    editor.setEntityAssetId(id, assetId);
    const auto name = editor.entityName(id);
    NOTO3D_INFO("M5_ASSET asset-assigned entity='{}' id={} assetId='{}'",
                name ? *name : "???", static_cast<unsigned>(id), assetId);
}

}  // namespace noto3d::app