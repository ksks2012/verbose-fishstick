#pragma once

#include <camera.hpp>
#include <camera_input.hpp>
#include <editor_shell.hpp>

struct GLFWwindow;

namespace noto3d::ui {

// Routes GLFW errors to the structured log.  Call immediately after
// glfwInit(), so init failures are reported rather than silently swallowed.
void installGlfwErrorCallback();

// Installs the window callbacks the editor needs and binds `input` as the
// scroll accumulator, so scroll ticks reach the camera input path.
//
// Call before the ImGui context is created: the GLFW ImGui backend chains the
// callbacks already registered here rather than replacing them.
void installWindowCallbacks(GLFWwindow* window, noto3d::app::InputState& input);

// Enables GL_DEBUG_OUTPUT and registers the debug-message callback when a debug
// context is available (ADR-0004).  No-op when built without NOTO3D_DEBUG_GL.
// Returns false when a debug context was requested but is unavailable.
[[nodiscard]] bool installGlDebugCallback();

// Samples the cursor and mouse buttons, then applies one frame of camera input
// under the ownership gate reported by `shell`.
//
// The scroll accumulator is drained unconditionally, so a delta denied by the
// gate is discarded rather than deferred to a later frame (ADR-0011 Q10).
void applyCameraInputForFrame(GLFWwindow*                   window,
                              noto3d::app::InputState&       input,
                              const noto3d::ui::ShellFrame& shell,
                              noto3d::renderer::Camera&      camera);

}  // namespace noto3d::ui
