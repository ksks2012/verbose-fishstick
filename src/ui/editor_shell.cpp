#include "editor_shell.hpp"

#include <viewport_rect.hpp>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "imgui.h"

namespace noto3d::ui {

ShellFrame submitEditorPanels(GLFWwindow* window,
                               noto3d::app::Editor& editor) {
    ShellFrame shell;

    // ── Hierarchy panel ──────────────────────────────────────────────────────
    // Lists entities in creation order; click-to-select (ADR-0011 Q3/Q11).
    ImGui::Begin("Hierarchy");
    for (const auto& item : editor.hierarchyItems()) {
        const bool isSelected = (item.id == editor.selectedId());
        if (ImGui::Selectable(item.name.c_str(), isSelected)) {
            editor.select(item.id);
        }
    }
    ImGui::End();

    // ── Inspector panel ─────────────────────────────────────────────────────
    // Shows selected entity's identity; no field editing (0030's job).
    ImGui::Begin("Inspector");
    if (editor.hasSelection()) {
        const auto name    = editor.selectedName();
        const auto assetId = editor.selectedAssetId();
        ImGui::Text("Name:    %s", name    ? name->c_str()    : "(none)");
        ImGui::Text("AssetId: %s", assetId ? assetId->c_str() : "(none)");
    } else {
        ImGui::TextDisabled("(nothing selected)");
    }
    ImGui::End();

    // ── Input-ownership inputs ─────────────────────────────────────────────
    // Read here, BEFORE the 3D panel adds any item — see the header contract.
    // These reflect all items in Hierarchy + Inspector built above; the 3D
    // panel's Dummy() below must not affect them.
    shell.anyItemHovered    = ImGui::IsAnyItemHovered();
    shell.anyItemActive     = ImGui::IsAnyItemActive();
    shell.globalModalCapture = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId |
                                                        ImGuiPopupFlags_AnyPopupLevel);

    // ── 3D View panel ──────────────────────────────────────────────────────
    // Transparent bg so the GL rendering shows through the panel.
    // NoScrollbar + NoScrollWithMouse: the wheel is owned by the camera gate.
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const ImGuiWindowFlags viewFlags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("3D View", nullptr, viewFlags);
    shell.viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    // Content region → GL-space rect.  The UI toolkit reports the content region
    // in window coordinates (origin top-left), while GL needs framebuffer pixels
    // with the origin at the bottom.
    //
    // The scale is derived from the GLFW window/framebuffer ratio rather than
    // read from the toolkit's density field: the GLFW backend pins that to 1.0
    // on X11, where the framebuffer may still be scaled (imgui_impl_glfw.cpp:
    // "Content Scales are always reported as 1.0 on Wayland. FramebufferScale
    // are always reported as 1.0 on X11.").
    const ImVec2 cpos  = ImGui::GetCursorScreenPos();
    const ImVec2 csize = ImGui::GetContentRegionAvail();
    int fbW = 0, fbH = 0, winW = 0, winH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glfwGetWindowSize(window, &winW, &winH);
    const ImGuiViewport* hostViewport = ImGui::GetMainViewport();
    shell.rect = noto3d::app::toFramebufferRect(
        noto3d::app::ScreenRect{cpos.x, cpos.y, csize.x, csize.y},
        noto3d::app::DisplayTransform{
            hostViewport->Pos.x, hostViewport->Pos.y,
            (winW > 0) ? static_cast<float>(fbW) / static_cast<float>(winW) : 1.0f,
            (winH > 0) ? static_cast<float>(fbH) / static_cast<float>(winH) : 1.0f,
            fbH});

    // Advance the cursor over the content area.  This DOES register an item.
    ImGui::Dummy(csize);
    ImGui::End();
    ImGui::PopStyleVar();

    return shell;
}

}  // namespace noto3d::ui
