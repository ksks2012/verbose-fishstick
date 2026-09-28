#pragma once

namespace noto3d::app {

// Pure predicate for 3D-view input ownership (ADR-0011 Q9/Q10).
//
// Returns true iff the camera may consume mouse drags and the scroll wheel
// on this frame.  All four inputs are Booleans sourced at the adapter edge
// (main.cpp / the ImGui wiring), never from io.WantCaptureMouse.
//
// !io.WantCaptureMouse must NOT be used as the gate — the 3D panel is itself
// an ImGui window, so that flag is permanently true and makes camera input dead.
//
// Wheel ownership: denied scroll deltas must be DISCARDED, never deferred to
// a later frame (ADR-0011 Q10 / ADR-0005 amendment).
[[nodiscard]] inline bool viewportGateOpen(bool viewportHovered,
                                            bool anyItemHovered,
                                            bool anyItemActive,
                                            bool globalModalCapture) noexcept {
    return viewportHovered && !anyItemHovered && !anyItemActive && !globalModalCapture;
}

}  // namespace noto3d::app
