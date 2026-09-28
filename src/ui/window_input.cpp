#include "window_input.hpp"

#include <log.hpp>

#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace noto3d::ui {
namespace {

void glfwErrorCallback(int code, const char* desc) {
    NOTO3D_ERROR("[glfw] error {}: {}", code, desc);
}

void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);  // DPI-safe viewport resize (ADR-0004)
}

void scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* input = static_cast<noto3d::app::InputState*>(glfwGetWindowUserPointer(window));
    if (input) {
        input->addScroll(yoffset);
    }
}

#if NOTO3D_DEBUG_GL
void GLAPIENTRY debugCallback(GLenum /*source*/, GLenum /*type*/, GLuint id,
                              GLenum severity, GLsizei /*length*/, const char* message,
                              const void* /*userParam*/) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;  // filter NOTIFICATION (ADR-0004)
    }
    const char* sev = (severity == GL_DEBUG_SEVERITY_HIGH)   ? "HIGH"
                    : (severity == GL_DEBUG_SEVERITY_MEDIUM) ? "MEDIUM"
                                                             : "LOW";
    NOTO3D_ERROR("[gl-debug][{}] id={}: {}", sev, id, message);
}
#endif

}  // namespace

void installGlfwErrorCallback() {
    glfwSetErrorCallback(glfwErrorCallback);
}

void installWindowCallbacks(GLFWwindow* window, noto3d::app::InputState& input) {
    glfwSetWindowUserPointer(window, &input);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);  // ADR-0004
    glfwSetScrollCallback(window, scrollCallback);                   // camera zoom (0023)
}

bool installGlDebugCallback() {
#if NOTO3D_DEBUG_GL
    if (GLAD_GL_KHR_debug && glDebugMessageCallback != nullptr) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugCallback, nullptr);
        return true;
    }
    NOTO3D_WARN("M0_WARN  debug context requested but GL_KHR_debug unavailable");
#endif
    return false;
}

void applyCameraInputForFrame(GLFWwindow*                   window,
                              noto3d::app::InputState&       input,
                              const noto3d::ui::ShellFrame& shell,
                              noto3d::renderer::Camera&      camera) {
    double curX = 0.0, curY = 0.0;
    glfwGetCursorPos(window, &curX, &curY);

    noto3d::app::CameraInputFrame frame;
    input.cursorDelta(curX, curY, frame.dx, frame.dy);

    frame.viewportHovered     = shell.viewportHovered;
    frame.anyItemHovered     = shell.anyItemHovered;
    frame.anyItemActive      = shell.anyItemActive;
    frame.globalModalCapture = shell.globalModalCapture;
    frame.orbitButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    frame.panButton   = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    // Always drain, so a denied delta is discarded and never deferred.
    frame.scrollTicks = input.takeScroll();

    noto3d::app::applyCameraInput(camera, frame);
}

}  // namespace noto3d::ui
