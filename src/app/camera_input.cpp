#include "camera_input.hpp"

#include "input_gate.hpp"

namespace noto3d::app {
namespace {

// Camera interaction sensitivities (app layer: pixels → world units / radians).
// Unit conversion is here, never inside Camera (ADR-0010 Q7 / ticket 0023).
constexpr float kOrbitSens = 0.005f;  // radians/pixel
constexpr float kPanSens   = 0.003f;  // world-units/pixel
constexpr float kZoomSens  = 0.5f;    // world-units/scroll-tick

}  // namespace

void InputState::addScroll(double ticks) noexcept {
    scrollY += static_cast<float>(ticks);
}

void InputState::cursorDelta(double x, double y, float& outDx, float& outDy) noexcept {
    if (firstMove) {
        // Establish the baseline only; the first frame must not move the camera.
        prevX     = x;
        prevY     = y;
        firstMove = false;
        outDx = 0.0f;
        outDy = 0.0f;
        return;
    }
    outDx = static_cast<float>(x - prevX);
    outDy = static_cast<float>(y - prevY);
    prevX = x;
    prevY = y;
}

float InputState::takeScroll() noexcept {
    const float ticks = scrollY;
    scrollY = 0.0f;
    return ticks;
}

void applyCameraInput(renderer::Camera& camera, const CameraInputFrame& frame) noexcept {
    if (!viewportGateOpen(frame.viewportHovered,
                          frame.anyItemHovered,
                          frame.anyItemActive,
                          frame.globalModalCapture)) {
        return;
    }
    if (frame.orbitButton) {
        camera.orbit(frame.dx * kOrbitSens, -frame.dy * kOrbitSens);
    }
    if (frame.panButton) {
        camera.pan(frame.dx * kPanSens, -frame.dy * kPanSens);
    }
    if (frame.scrollTicks != 0.0f) {
        camera.zoom(frame.scrollTicks * kZoomSens);
    }
}

}  // namespace noto3d::app
