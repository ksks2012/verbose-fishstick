#pragma once

#include <asset_picker.hpp>
#include <editor.hpp>
#include <scene.hpp>

#include <string>

namespace noto3d::app {

// M5 create/destroy commit utilities (ticket 0032).  The discrete UI widgets
// (Hierarchy Create / Destroy) call these; each does the mutation through the
// Editor facade and logs it on the event (ADR-0011 Q8 — never per frame).

// Creates an entity through the facade and returns its new id.
//
// The assetId is TAKEN FROM AN AssetChoice (type-enforced, not an arbitrary
// string), so a created entity is drawable by construction — the choice value
// only ever comes from the picker's renderable-only list (ADR-0011 Q7).  It
// never starts life as an unresolvable entity (ticket 0032).
//
// M6 identity contract: the returned id is monotonic and never reused; emptying
// the Scene (clear()/destroy-all) does not reset the counter (ADR-0010 Q4).
[[nodiscard]] scene::EntityId commitCreateEntity(Editor& editor,
                                                 const AssetChoice& choice);

// Destroys the currently selected entity (if any) through the facade, logs the
// destroyed id, and clears the selection.  A no-op when nothing is selected —
// no crash, no error log (ticket 0032; no undo, no destroy+recreate, ADR-0011 Q4).
void commitDestroySelected(Editor& editor);

}  // namespace noto3d::app