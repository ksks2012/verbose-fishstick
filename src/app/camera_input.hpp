#pragma once

#include <camera.hpp>

namespace noto3d::app {

// Per-frame mouse state for camera interaction (ADR-0010 Q7: no Input
// abstraction; the adapter feeds this struct raw device state).
//
// Owns the cursor baseline so that the first sample after focus or a widget
// drag does not produce a spurious jump.  Ticket 0026 formalises the
// re-baseline policy; what exists here is the M4 behaviour, unchanged.
struct InputState {
    double prevX     = 0.0;
    double prevY     = 0.0;
    float  scrollY   = 0.0f;
    bool   firstMove = true;

    // Accumulates scroll ticks from the windowing layer's scroll callback.
    void addScroll(double ticks) noexcept;

    // Cursor delta in pixels since the previous sample, updating the baseline.
    // The first sample only establishes the baseline and returns {0, 0}.
    void cursorDelta(double x, double y, float& outDx, float& outDy) noexcept;

    // Reads the accumulated scroll and clears it.  Callers ALWAYS drain this,
    // so a delta denied by the input-ownership gate is discarded rather than
    // deferred to a later frame (ADR-0011 Q10).
    [[nodiscard]] float takeScroll() noexcept;
};

// One frame of camera input: ownership gate inputs plus device state.
//
// Pure value type — no windowing or UI-toolkit types, so the whole gesture
// mapping stays headless-testable in noto3d_app.
struct CameraInputFrame {
    // Gate inputs, sourced by the UI adapter (ADR-0011 Q10).
    bool viewportHovered    = false;
    bool anyItemHovered    = false;
    bool anyItemActive     = false;
    bool globalModalCapture = false;

    // Device state for this frame.
    float dx           = 0.0f;   // cursor delta, pixels
    float dy           = 0.0f;
    float scrollTicks  = 0.0f;   // already drained from InputState
    bool  orbitButton  = false;  // left button held
    bool  panButton    = false;  // middle button held
};

// Applies one frame of camera input.
//
// Ownership is decided by viewportGateOpen() on the four gate inputs; a closed
// gate consumes nothing, so hovering a panel, dragging a widget, or a modal
// capture leaves the camera untouched.
//
// Sensitivity and unit conversion live here, never inside renderer::Camera
// (ADR-0010 Q7 / ticket 0023).  Ticket 0026 owns the gesture→operation mapping
// and sensitivity configuration; these values are the M4 behaviour, unchanged.
void applyCameraInput(renderer::Camera& camera, const CameraInputFrame& frame) noexcept;

}  // namespace noto3d::app
