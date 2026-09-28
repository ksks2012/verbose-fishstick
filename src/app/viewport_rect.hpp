#pragma once

namespace noto3d::app {

// GL-space viewport rect (framebuffer pixels; origin at bottom-left, Y up).
//
// ADR-0011 Q0 coordinate contract:
//   * This struct holds ONLY the already-converted GL-space values.
//   * The Y-flip from ImGui screen space (origin top-left, Y down) is applied
//     in the UI adapter (main.cpp), never here.
//   * No ImGui types are allowed in this header.
struct ViewportRect {
    int x      = 0;  // GL-space left edge
    int y      = 0;  // GL-space bottom edge
    int width  = 1;
    int height = 1;

    // width / height; returns 1.0f when height == 0 (degenerate guard).
    [[nodiscard]] float aspect() const noexcept {
        return (height > 0)
            ? static_cast<float>(width) / static_cast<float>(height)
            : 1.0f;
    }

    // True if the GL-space point (cx, cy) lies within this rect.
    // Left/bottom edges are inclusive; right/top edges are exclusive.
    [[nodiscard]] bool contains(float cx, float cy) const noexcept {
        return cx >= static_cast<float>(x)            &&
               cx <  static_cast<float>(x + width)   &&
               cy >= static_cast<float>(y)            &&
               cy <  static_cast<float>(y + height);
    }

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0;
    }
};

}  // namespace noto3d::app
