#pragma once

#include <viewport_rect.hpp>

struct GLFWwindow;

namespace noto3d::ui {

// What the editor's panels report for one frame.
struct ShellFrame {
    // GL-space rect of the 3D panel, in framebuffer pixels.  Invalid when the
    // panel has no content area or the window has no framebuffer.
    noto3d::app::ViewportRect rect;

    // The four input-ownership inputs (ADR-0011 Q10).  Sourced here because
    // they are UI-toolkit state; consumed by app::viewportGateOpen().
    bool viewportHovered     = false;
    bool anyItemHovered     = false;
    bool anyItemActive      = false;
    bool globalModalCapture = false;
};

// Submits the editor's panels for one frame and reports the 3D panel's extent.
//
// *** ORDER INSIDE THIS FUNCTION IS A CONTRACT, NOT A STYLE CHOICE ***
//
// The four ownership inputs are read BEFORE the 3D panel submits its own
// items.  ImGui::Dummy() calls ItemAdd(), so reading the gate after the panel
// reserves its content area would report an item under the cursor on every
// frame the cursor is over the 3D view, closing the gate permanently —
// silently, with every test still green, because the predicate is correct and
// only its evaluation site is wrong (ADR-0011 Q10; ticket 0034 ②).
//
// Hierarchy and Inspector are empty placeholders in this slice.  When they gain
// widgets, those widgets are submitted before the reads above precisely so they
// arbitrate against the camera.
[[nodiscard]] ShellFrame submitEditorPanels(GLFWwindow* window);

}  // namespace noto3d::ui
