#include "viewport_rect.hpp"

#include <cmath>

namespace noto3d::app {

float ViewportRect::aspect() const noexcept {
    return (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
}

bool ViewportRect::valid() const noexcept {
    return width > 0 && height > 0;
}

bool ViewportRect::contains(float cx, float cy) const noexcept {
    return cx >= static_cast<float>(x)          && cx < static_cast<float>(x + width) &&
           cy >= static_cast<float>(y)          && cy < static_cast<float>(y + height);
}

ViewportRect toFramebufferRect(const ScreenRect&     screen,
                               const DisplayTransform& display) noexcept {
    const float sx = (display.scaleX > 0.0f) ? display.scaleX : 1.0f;
    const float sy = (display.scaleY > 0.0f) ? display.scaleY : 1.0f;
    const float left   = (display.originX + screen.x) * sx;
    const float top    = (display.originY + screen.y) * sy;
    const float width  = screen.width  * sx;
    const float height = screen.height * sy;

    // Minimised window: no framebuffer to express a rect in, so report an
    // invalid rect and let the caller skip the 3D pass.
    if (display.framebufferHeight <= 0) {
        return ViewportRect{0, 0, 0, 0};
    }

    return ViewportRect{
        static_cast<int>(std::lround(left)),
        // Y-flip (ADR-0011 Q0): gl_y = fbH - (framebuffer_top + height)
        static_cast<int>(std::lround(static_cast<float>(display.framebufferHeight) - (top + height))),
        static_cast<int>(std::lround(width)),
        static_cast<int>(std::lround(height))
    };
}

}  // namespace noto3d::app
