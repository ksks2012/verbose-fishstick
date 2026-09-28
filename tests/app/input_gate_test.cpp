#include "input_gate.hpp"
#include "log.hpp"

#include <cstdlib>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-input-gate-tests");
    using noto3d::app::viewportGateOpen;

    // ── gate OPEN: only when all four conditions satisfied ───────────────────
    check( viewportGateOpen(true,  false, false, false),
          "gate open: hovered, no item hovered, no item active, no modal");

    // ── gate CLOSED: any single condition violated ────────────────────────────
    check(!viewportGateOpen(false, false, false, false),
          "gate closed: viewport NOT hovered");
    check(!viewportGateOpen(true,  true,  false, false),
          "gate closed: anyItemHovered");
    check(!viewportGateOpen(true,  false, true,  false),
          "gate closed: anyItemActive");
    check(!viewportGateOpen(true,  false, false, true),
          "gate closed: globalModalCapture");

    // ── gate CLOSED: two conditions violated ─────────────────────────────────
    check(!viewportGateOpen(false, true,  false, false),
          "gate closed: not hovered + anyItemHovered");
    check(!viewportGateOpen(false, false, true,  false),
          "gate closed: not hovered + anyItemActive");
    check(!viewportGateOpen(false, false, false, true),
          "gate closed: not hovered + globalModalCapture");
    check(!viewportGateOpen(true,  true,  true,  false),
          "gate closed: anyItemHovered + anyItemActive");
    check(!viewportGateOpen(true,  true,  false, true),
          "gate closed: anyItemHovered + globalModalCapture");
    check(!viewportGateOpen(true,  false, true,  true),
          "gate closed: anyItemActive + globalModalCapture");

    // ── gate CLOSED: three conditions violated ────────────────────────────────
    check(!viewportGateOpen(false, true,  true,  false), "gate closed: 3 violated A");
    check(!viewportGateOpen(false, true,  false, true),  "gate closed: 3 violated B");
    check(!viewportGateOpen(false, false, true,  true),  "gate closed: 3 violated C");
    check(!viewportGateOpen(true,  true,  true,  true),  "gate closed: all violated");

    // ── all 16 combinations exhaustively (ADR-0011 Q10 both polarities) ──────
    // Exactly one combination is open: (true, false, false, false)
    int openCount = 0;
    for (int h = 0; h <= 1; ++h)
    for (int ih = 0; ih <= 1; ++ih)
    for (int ia = 0; ia <= 1; ++ia)
    for (int mc = 0; mc <= 1; ++mc) {
        if (viewportGateOpen(h != 0, ih != 0, ia != 0, mc != 0)) {
            ++openCount;
        }
    }
    check(openCount == 1, "exactly 1 of 16 combinations opens the gate");

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
