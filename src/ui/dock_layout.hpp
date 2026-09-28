#pragma once

namespace noto3d::ui {

// The invisible full-window dock host plus its programmatic default layout.
//
// Layout is applied once, on the first submitted frame, and never persisted
// (ImGui's .ini is disabled), so every run starts from the same arrangement:
// Hierarchy left, Inspector right, 3D View in the remainder.  Subsequent frames
// only submit the host, so the user can dock and undock freely afterwards.
//
// This type holds presentation state only; it does not read or write any
// editor or scene state.
class DockLayout {
public:
    // Submits the dock host for one frame, applying the default layout on the
    // first call.  Must run between ImGuiHarness::newFrame() and the panels.
    void submitDockHost();
};

}  // namespace noto3d::ui
