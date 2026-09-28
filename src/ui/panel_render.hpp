#pragma once

#include <camera.hpp>
#include <draw_list.hpp>
#include <renderer.hpp>
#include <scene.hpp>
#include <viewport_state.hpp>

struct GLFWwindow;

namespace noto3d::ui {

// Restricts GL viewport *and* scissor to the 3D panel, so both the draw calls
// and the Begin3DPass() clear stay inside it and the rest of the window is left
// to the UI toolkit (ADR-0005 amendment; ADR-0011 Q0).
//
// A default-constructed instance is inactive, so an invalid rect (minimised
// window, collapsed panel) simply skips the scope instead of needing a branch
// at every call site.
class PanelViewport {
public:
    explicit PanelViewport(const noto3d::app::ViewportRect& rect) noexcept;
    ~PanelViewport();

    PanelViewport(const PanelViewport&)            = delete;
    PanelViewport& operator=(const PanelViewport&) = delete;

    [[nodiscard]] bool active() const noexcept { return active_; }

private:
    bool active_ = false;
};

// Restores the full-framebuffer viewport after a PanelViewport scope, ready for
// the UI toolkit's own pass.
void restoreFullFramebufferViewport(GLFWwindow* window);

// Draws one frame of scene content into the 3D panel.
//
// Rendering still goes exclusively through app::collectDrawItems() — there is
// no direct submit bypass (ADR-0010; ticket 0022).  A no-op when `view.rect`
// is invalid.
void renderPanel3D(GLFWwindow*                   window,
                   const noto3d::app::ViewportState& view,
                   const noto3d::scene::Scene&    scene,
                   const noto3d::app::Resolver&    resolve,
                   noto3d::renderer::Renderer&     renderer,
                   noto3d::renderer::ShaderProgram&       shader);

}  // namespace noto3d::ui
