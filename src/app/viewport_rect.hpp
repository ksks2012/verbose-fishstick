#pragma once

namespace noto3d::app {

// GL-space viewport rect (framebuffer pixels; origin at bottom-left, Y up).
//
// ADR-0011 Q0 coordinate contract:
//   * This struct holds ONLY already-converted GL-space values.
//   * The Y-flip from the UI toolkit's screen space (origin top-left, Y down)
//     is applied while producing the rect, by toFramebufferRect() below.
//   * No UI-toolkit types are allowed in this header.
//
// Declaration-only by project rule: every definition lives in viewport_rect.cpp.
struct ViewportRect {
    int x      = 0;  // GL-space left edge
    int y      = 0;  // GL-space bottom edge
    int width  = 1;
    int height = 1;

    // width / height; returns 1.0f when height <= 0 (degenerate guard).
    [[nodiscard]] float aspect() const noexcept;

    [[nodiscard]] bool valid() const noexcept;

    // True if the GL-space point (cx, cy) lies within this rect.
    // Left/bottom edges are inclusive; right/top edges are exclusive.
    //
    // NOT on the input-ownership path: ownership is decided by the UI toolkit's
    // hover and item state (ADR-0011 Q10), never by converting a cursor
    // position.  Reserved for consumers that need a point test in framebuffer
    // space (0026 cursor-baseline policy, future picking).  cx/cy are
    // framebuffer pixels — NOT window coordinates and NOT logical/scaled
    // pixels; a caller holding a window-space point must apply
    // DisplayTransform's origin and scale first (see ticket 0034 ①).
    [[nodiscard]] bool contains(float cx, float cy) const noexcept;
};

// A rect in UI-adapter space: origin top-left, Y growing downward, measured in
// logical (unscaled) pixels.  This is the space the UI toolkit reports the
// content rect in.
struct ScreenRect {
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;
};

// Maps ScreenRect (UI-adapter space) into framebuffer pixels.  Every field
// comes from the windowing layer once per frame; all four matter:
//   originX/originY   host viewport offset in the screen
//   scaleX/scaleY     logical px -> framebuffer px
//   framebufferHeight framebuffer height in px, for the GL Y-flip
//
// scaleX/scaleY are meant to be the window/framebuffer ratio reported by the
// windowing layer (framebufferSize / windowSize), NOT the UI toolkit's
// density field: the GLFW backend pins its density to 1.0 on X11 even when the
// framebuffer is scaled, so trusting it would leave HiDPI wrong.
struct DisplayTransform {
    float originX           = 0.0f;
    float originY           = 0.0f;
    float scaleX            = 1.0f;
    float scaleY            = 1.0f;
    int   framebufferHeight = 0;
};

// ScreenRect + DisplayTransform -> GL-space ViewportRect.
//
// Applying the scale and the origin is what makes the rect correct on a HiDPI
// display or on a window that is not at the screen origin.  Omitting either
// happens to be harmless at scale 1.0 with the window at (0,0) — which is the
// common case — and silently yields a wrong aspect ratio and a wrong scissor
// box everywhere else, so the whole transform is applied here rather than at
// the call site (ADR-0011 Q0; see ticket 0028).
//
// Returns an invalid rect when there is no framebuffer (minimised window), so
// callers can keep using valid() to skip the 3D pass.
[[nodiscard]] ViewportRect toFramebufferRect(const ScreenRect&     screen,
                                             const DisplayTransform& display) noexcept;

}  // namespace noto3d::app
