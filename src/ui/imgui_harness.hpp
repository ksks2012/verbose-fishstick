#pragma once

struct GLFWwindow;

namespace noto3d::ui {

// RAII owner of the ImGui context and its GLFW / OpenGL3 backends.
//
// The context is created while a GL context is current and destroyed before it
// is, so every ImGui GL resource dies in the right order (ticket 0025's scope
// rule).  Construct after the GL context exists; the destructor is the only
// teardown path, so no early return can leak it.
//
// Docking is enabled and the .ini file is disabled, because the layout is set
// programmatically by DockLayout (no persisted user layout yet).
class ImGuiHarness {
public:
    // Throws std::runtime_error if the context or a backend fails to initialise.
    explicit ImGuiHarness(GLFWwindow* window);
    ~ImGuiHarness();

    ImGuiHarness(const ImGuiHarness&)            = delete;
    ImGuiHarness& operator=(const ImGuiHarness&) = delete;

    // Starts a frame: backend new-frame calls, then ImGui::NewFrame().
    void newFrame();

    // Ends a frame: ImGui::Render() then the OpenGL3 draw-data submission.
    void render();

private:
    GLFWwindow* window_ = nullptr;
};

}  // namespace noto3d::ui
