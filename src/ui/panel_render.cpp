#include "panel_render.hpp"

#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

namespace noto3d::ui {

PanelViewport::PanelViewport(const noto3d::app::ViewportRect& rect) noexcept {
    if (!rect.valid()) {
        return;
    }
    // Scissor first, then viewport: both must be in place before Begin3DPass()
    // clears, otherwise the clear escapes the panel.
    glEnable(GL_SCISSOR_TEST);
    glScissor(rect.x, rect.y, rect.width, rect.height);
    glViewport(rect.x, rect.y, rect.width, rect.height);
    active_ = true;
}

PanelViewport::~PanelViewport() {
    if (active_) {
        glDisable(GL_SCISSOR_TEST);
    }
}

void restoreFullFramebufferViewport(GLFWwindow* window) {
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
}

void renderPanel3D(GLFWwindow*                      window,
                   const noto3d::app::ViewportState& view,
                   const noto3d::scene::Scene&     scene,
                   const noto3d::app::Resolver&     resolve,
                   noto3d::renderer::Renderer&      renderer,
                   noto3d::renderer::ShaderProgram&       shader) {
    if (!view.rect.valid()) {
        return;
    }

    const PanelViewport scoped(view.rect);

    const auto proj = view.camera.projectionMatrix(view.rect.aspect());
    const auto viewMat = view.camera.viewMatrix();

    renderer.Begin3DPass();  // clears within the scissor, sets 3D state
    for (const auto& item : noto3d::app::collectDrawItems(scene, resolve)) {
        const glm::mat4 mvp = proj * viewMat * item.modelMatrix;
        for (const auto& sm : *item.model) {
            renderer.submit(sm.mesh, sm.material, shader, mvp);
        }
    }
    renderer.End3DPass();

    restoreFullFramebufferViewport(window);
}

}  // namespace noto3d::ui
