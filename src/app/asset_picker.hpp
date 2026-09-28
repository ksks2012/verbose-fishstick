#pragma once

#include <asset_registry.hpp>
#include <editor.hpp>
#include <manifest_types.hpp>
#include <scene.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace noto3d::app {

// One selectable row of the M5 asset picker (ADR-0011 Q7).  Carries TYPED
// values — id + typed geometry source — never a pre-formatted display string:
// turning them into user-facing text is presentation work in the UI adapter
// (same principle that fixes unit conversion there, Q13).  Never a pointer
// into registry storage: the snapshot is a value copy the UI may freely keep.

struct AssetChoice {
    std::string            id;
    asset::GeometrySource  geometrySource;
};

// Value snapshot consumed by the picker (ADR-0011 Q7).  Built by the app layer
// from AssetRegistry::entries(); the UI must not hold a std::span into registry
// storage.  No cache, no generation counter, no version token — the list is
// rebuilt from the registry when the picker needs it.
//
// renderableCount == choices.size(); hiddenCount == geometry-less /
// reference-only entries dropped by the renderable-only rule.  Both are
// reported to the caller (and logged) so hiding is observable, never silent.
struct AssetPickerSnapshot {
    std::vector<AssetChoice> choices;
    std::size_t              renderableCount = 0;
    std::size_t              hiddenCount     = 0;
};

// Rebuild the picker snapshot from the registry's current entries.
//
//   * renderable = geometry present (geometrySource != null); entries without
//     geometry (reference-only / geometry-less) are hidden, not offered.
//   * Emits the structured diagnostic `M5_ASSET asset-picker n=<renderable>
//     hidden=<reference-only>` so hiding is observable in the log (Q7/Q8).
[[nodiscard]] AssetPickerSnapshot buildAssetPickerSnapshot(const asset::AssetRegistry& registry);

// Commitment of a picker choice: the ONLY code path that reassigns an entity's
// assetId from the picker.  Goes through the Editor facade (EntityId-keyed,
// ADR-0011 Q3/Q11), mutates only `Entity::assetId` (no Model/GPU ownership, Q1),
// and logs the assignment.  An id that later fails to resolve degrades to the
// existing skip-at-frame behaviour (ADR-0010 Q5), never a crash or an empty list.
void commitAssetAssignment(Editor& editor, scene::EntityId id, std::string assetId);

}  // namespace noto3d::app