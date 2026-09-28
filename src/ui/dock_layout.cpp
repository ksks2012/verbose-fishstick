#include "dock_layout.hpp"

#include "imgui.h"
#include "imgui_internal.h"  // DockBuilder* (internal API; see ticket 0034 follow-up)

namespace noto3d::ui {
namespace {

// Window names are the contract with DockLayout's default split below; keep the
// two in step.
constexpr const char* kHierarchyName = "Hierarchy";
constexpr const char* kInspectorName = "Inspector";
constexpr const char* kViewName      = "3D View";

}  // namespace

void DockLayout::submitDockHost() {
    // First-frame default layout (programmatic; no .ini) --------------------
    static bool firstLayout = true;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_NoTitleBar   |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize     |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::Begin("##DockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);
    const ImGuiID dsID = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dsID, ImVec2(0, 0));

    if (firstLayout) {
        ImGui::DockBuilderRemoveNode(dsID);
        ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dsID, vp->Size);

        ImGuiID left = 0, right = 0;
        ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Left, 0.20f, &left, &right);
        ImGuiID inspector = 0, view = 0;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.25f, &inspector, &view);

        ImGui::DockBuilderDockWindow(kHierarchyName, left);
        ImGui::DockBuilderDockWindow(kViewName, view);
        ImGui::DockBuilderDockWindow(kInspectorName, inspector);
        ImGui::DockBuilderFinish(dsID);
        firstLayout = false;
    }
    ImGui::End();
}

}  // namespace noto3d::ui
