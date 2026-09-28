#include "editor_shell.hpp"

#include <entity_edit.hpp>
#include <transform_edit.hpp>
#include <viewport_rect.hpp>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "imgui.h"

namespace noto3d::ui {

// Display text for a typed geometry source — presentation work, done here in
// the UI adapter from the snapshot's TYPED value (ADR-0011 Q7/Q13), never
// pre-formatted by the app layer.
const char* geometrySourceLabel(asset::GeometrySource source) {
    return source == asset::GeometrySource::Placeholder ? "placeholder" : "official";
}

// Display label for one picker choice: id + source in parentheses.  Used by the
// Inspector picker and the Hierarchy Create control.
std::string choiceLabel(const noto3d::app::AssetChoice& choice) {
    return choice.id + " (" + geometrySourceLabel(choice.geometrySource) + ")";
}

ShellFrame submitEditorPanels(GLFWwindow* window,
                               noto3d::app::Editor& editor,
                               const noto3d::app::AssetPickerSnapshot& picker) {
    ShellFrame shell;

    // ── Hierarchy panel ──────────────────────────────────────────────────────
    // Entity list (creation order; click-to-select) + create/destroy controls
    // (ticket 0032).  The create-asset choice comes from the picker's choices,
    // so a new entity is drawable by construction (ADR-0011 Q7); the created
    // entity's name defaults to its asset id (Scene names need not be unique).
    //
    // The chosen create-asset selection is UI-session presentation state: a
    // single int in THIS WINDOW's ImGui storage.  Window-scoped (revoked with
    // the window, cleared between frames is what Begin()/End() storage does),
    // NOT a process-global static — ADR-0011 Q6.  We store an index into the
    // picker snapshot; it is re-clamped against the current snapshot every
    // frame and the choice OBJECT is re-resolved at click time, so a rebuild
    // (selection-change re-filter) can never feed Create a stale binding.
    ImGui::Begin("Hierarchy");
    {
        const ImGuiID key = ImGui::GetID("createAssetIndex");
        int* const indexPtr = ImGui::GetStateStorage()->GetIntRef(key, 0);

        if (!picker.choices.empty()) {
            int index = *indexPtr;
            if (index < 0 || index >= static_cast<int>(picker.choices.size())) {
                index = 0;  // snapshot (re)built → default to the first choice
                *indexPtr = index;
            }

            const auto& createChoice = picker.choices[static_cast<std::size_t>(index)];
            const std::string createPreview = choiceLabel(createChoice);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("Create asset", createPreview.c_str())) {
                for (int i = 0; i < static_cast<int>(picker.choices.size()); ++i) {
                    const auto& c = picker.choices[static_cast<std::size_t>(i)];
                    const std::string label = choiceLabel(c);
                    if (ImGui::Selectable(label.c_str(), i == index)) {
                        *indexPtr = i;
                        index = i;
                    }
                }
                ImGui::EndCombo();
            }
            // Resolve the choice OBJECT at click time from THIS frame's snapshot
            // (never a reference to a choice captured/stale from an earlier
            // frame).  index above is already re-clamped this frame.
            if (ImGui::Button("Create")) {
                // New id is deliberately not retained: create does not
                // auto-select (ticket 0032); the row appears above now.
                (void)noto3d::app::commitCreateEntity(
                    editor, picker.choices[static_cast<std::size_t>(index)]);
            }
        } else {
            ImGui::TextDisabled("(no renderable assets — create unavailable)");
        }
    }
    ImGui::Separator();
    for (const auto& item : editor.hierarchyItems()) {
        const bool isSelected = (item.id == editor.selectedId());
        // Unique widget id per entity (###<id>): Selectable() ids must not
        // collide when two entities share a name (create defaults the name to
        // the asset id, so two same-asset entities WOULD collide otherwise).
        const std::string rowLabel = item.name + "###" + std::to_string(item.id);
        if (ImGui::Selectable(rowLabel.c_str(), isSelected)) {
            editor.select(item.id);
        }
    }
    ImGui::Separator();
    // BeginDisabled/EndDisabled must ALWAYS pair (ImGui asserts on an
    // unbalanced EndDisabled).  Capture the selection ONCE — re-reading it after
    // commitDestroySelected destructively cleared it would unbalance the stack.
    const bool canDestroy = editor.hasSelection();
    ImGui::BeginDisabled(!canDestroy);
    if (ImGui::Button("Destroy selected")) {
        noto3d::app::commitDestroySelected(editor);
    }
    ImGui::EndDisabled();
    ImGui::End();

    // ── Inspector panel ─────────────────────────────────────────────────────
    // Identity lines stay (0029); transform fields added here (0030).
    // The Scene value is the widget's source of truth every frame:
    //   read → convert to degrees → feed widget → commit on IsItemDeactivatedAfterEdit.
    // No persistent degrees copy is kept in UI state (ADR-0011 Q13).
    ImGui::Begin("Inspector");
    if (editor.hasSelection()) {
        const auto id        = editor.selectedId();
        const auto name      = editor.selectedName();
        const auto assetId   = editor.selectedAssetId();
        const auto transform = editor.selectedTransform();

        ImGui::Text("Name:    %s", name    ? name->c_str()    : "(none)");
        ImGui::Text("AssetId: %s", assetId ? assetId->c_str() : "(none)");

        // ── Asset picker (0031) ─────────────────────────────────────────
        // Consumes the app-layer VALUE snapshot; display text is derived here
        // (geometrySource → label), never held as a view into the registry.
        // Discrete widget: commit (and its log) happen on the selection event.
        ImGui::Separator();
        if (picker.choices.empty()) {
            ImGui::TextDisabled("(no renderable assets available)");
        } else {
            const char* preview = "?";
            std::string previewBuf;
            if (assetId) {
                previewBuf = *assetId;
                preview = previewBuf.c_str();
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("Asset##picker", preview)) {
                for (const auto& choice : picker.choices) {
                    const bool isCurrent = assetId && *assetId == choice.id;
                    if (ImGui::Selectable(choiceLabel(choice).c_str(), isCurrent)) {
                        noto3d::app::commitAssetAssignment(editor, id, choice.id);
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (transform) {
            const noto3d::scene::Transform& t = *transform;

            // ── Position ───────────────────────────────────────────────
            ImGui::Separator();
            float pos[3] = {t.position.x, t.position.y, t.position.z};
            ImGui::DragFloat3("Position", pos, 0.01f);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                noto3d::app::commitPosition(editor, id, {pos[0], pos[1], pos[2]});
            }

            // ── Rotation (degrees; Scene stores radians — ADR-0011 Q13) ──
            // modelMatrix composes eulerAngleYXZ(y,x,z); label each axis
            // by its role so the applied order is visible, not implied.
            ImGui::Separator();
            ImGui::TextUnformatted("Rotation (Y->X->Z applied order):");
            const glm::vec3 rotDeg =
                noto3d::app::rotationRadiansToDegrees(t.rotation);
            float pitch = rotDeg.x;   // Pitch: rotation.x
            float yaw   = rotDeg.y;   // Yaw:   rotation.y
            float roll  = rotDeg.z;   // Roll:  rotation.z

            // Separate DragFloat per axis: each fires its own commit boundary.
            ImGui::DragFloat("  Pitch (X)##rotX", &pitch, 0.5f, -360.0f, 360.0f, "%.2f deg");
            const bool pitchDone = ImGui::IsItemDeactivatedAfterEdit();
            ImGui::DragFloat("  Yaw   (Y)##rotY", &yaw,   0.5f, -360.0f, 360.0f, "%.2f deg");
            const bool yawDone   = ImGui::IsItemDeactivatedAfterEdit();
            ImGui::DragFloat("  Roll  (Z)##rotZ", &roll,  0.5f, -360.0f, 360.0f, "%.2f deg");
            const bool rollDone  = ImGui::IsItemDeactivatedAfterEdit();

            if (pitchDone || yawDone || rollDone) {
                // Commit once, on release, with the final value (Q8).
                // commitRotation converts degrees → radians and logs both (Q13).
                noto3d::app::commitRotation(editor, id, {pitch, yaw, roll});
            }

            // ── Scale ──────────────────────────────────────────────────
            ImGui::Separator();
            float scale[3] = {t.scale.x, t.scale.y, t.scale.z};
            ImGui::DragFloat3("Scale", scale, 0.01f, 0.001f, 100.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                noto3d::app::commitScale(editor, id, {scale[0], scale[1], scale[2]});
            }
        }
    } else {
        ImGui::TextDisabled("(nothing selected)");
    }

    // ── Frame-rate readout ─────────────────────────────────────────────────
    // ImGui's built-in smoothed FPS (averaged over recent frames); presentational
    // only, in the UI adapter.  No app-layer counter and no headless impact.
    ImGui::Separator();
    const ImGuiIO& fps_io = ImGui::GetIO();
    if (fps_io.Framerate > 0.0f) {
        ImGui::Text("FPS: %.1f (frame %.2f ms)", fps_io.Framerate, 1000.0f / fps_io.Framerate);
    } else {
        ImGui::TextDisabled("FPS: --");
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
