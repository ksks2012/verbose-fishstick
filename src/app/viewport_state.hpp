#pragma once

#include "viewport_rect.hpp"
#include <camera.hpp>

namespace noto3d::app {

// Per-view state: the geometric extent of the view and the camera for it.
//
// Lives in the render scope alongside the Scene (ADR-0011 Q6).
// Not owned by Scene — Scene is pure data (ADR-0010 Q1).
// The camera is stored here rather than as a naked local so the lifetime is
// explicit: the camera is valid for exactly the lifetime of the view it serves.
struct ViewportState {
    ViewportRect             rect;    // updated each frame from the panel content region
    noto3d::renderer::Camera camera;  // initial state = M4 camera (ticket 0023)
};

}  // namespace noto3d::app
