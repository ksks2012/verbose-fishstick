#include "imgui_harness.hpp"

#include <log.hpp>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace noto3d::ui {

ImGuiHarness::ImGuiHarness(GLFWwindow* window) : window_(window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename  = nullptr;  // layout set programmatically; no ini file
    ImGui::StyleColorsDark();

    // The GLFW backend installs its own callbacks and chains the ones already
    // registered on the window, so window_input's scroll accumulator survives.
    if (ImGui_ImplGlfw_InitForOpenGL(window_, true) == 0) {
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui_ImplGlfw_InitForOpenGL failed");
    }
    if (ImGui_ImplOpenGL3_Init("#version 330 core") == 0) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui_ImplOpenGL3_Init failed");
    }
}

ImGuiHarness::~ImGuiHarness() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiHarness::newFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiHarness::render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace noto3d::ui
