#include "viewport_rect.hpp"
#include "log.hpp"

#include <cmath>
#include <cstdlib>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

bool near(float a, float b) { return std::abs(a - b) < 1e-5f; }

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-viewport-rect-tests");
    using noto3d::app::ViewportRect;

    // ── aspect ratio ─────────────────────────────────────────────────────────
    {
        const ViewportRect r{0, 0, 1280, 720};
        check(near(r.aspect(), 1280.0f / 720.0f), "aspect: 1280x720");
    }
    {
        const ViewportRect r{0, 0, 400, 400};
        check(near(r.aspect(), 1.0f), "aspect: square");
    }
    {
        const ViewportRect r{0, 0, 16, 9};
        check(near(r.aspect(), 16.0f / 9.0f), "aspect: non-square 16x9");
    }
    {
        const ViewportRect r{0, 0, 1, 0};
        check(near(r.aspect(), 1.0f), "aspect: degenerate height=0 → 1.0");
    }

    // ── valid ─────────────────────────────────────────────────────────────────
    {
        check(ViewportRect{0, 0, 100, 100}.valid(),  "valid: 100x100");
        check(!ViewportRect{0, 0, 0,   100}.valid(), "valid: width=0 → invalid");
        check(!ViewportRect{0, 0, 100, 0  }.valid(), "valid: height=0 → invalid");
        check(!ViewportRect{0, 0, -1,  100}.valid(), "valid: negative width → invalid");
    }

    // ── contains ─────────────────────────────────────────────────────────────
    //   rect at (10,20), size 100x80 — covers x:[10,110), y:[20,100)
    {
        const ViewportRect r{10, 20, 100, 80};

        // Centre
        check(r.contains(60.0f, 60.0f), "contains: centre");

        // Corners — inclusive
        check(r.contains(10.0f, 20.0f), "contains: bottom-left corner (inclusive)");
        check(r.contains(10.0f, 99.0f), "contains: top-left inside");
        check(r.contains(109.0f, 20.0f), "contains: bottom-right inside");
        check(r.contains(109.9f, 99.9f), "contains: top-right inside");

        // Edges — exclusive right and top
        check(!r.contains(110.0f, 60.0f), "contains: right edge exclusive");
        check(!r.contains(60.0f, 100.0f), "contains: top edge exclusive");

        // Fully outside
        check(!r.contains(9.9f,  60.0f), "contains: left of rect");
        check(!r.contains(60.0f, 19.9f), "contains: below rect");
        check(!r.contains(200.0f, 60.0f), "contains: far right");
        check(!r.contains(60.0f, 200.0f), "contains: far above");
    }

    // ── non-square panel (ticket 0028 acceptance: non-square rect) ────────────
    {
        // Tall narrow panel: x=200, y=0, w=100, h=400
        const ViewportRect r{200, 0, 100, 400};
        check(near(r.aspect(), 100.0f / 400.0f), "non-square: narrow aspect");
        check(r.contains(200.0f, 0.0f),   "non-square: contains bottom-left");
        check(r.contains(299.0f, 399.0f), "non-square: contains top-right inside");
        check(!r.contains(199.0f, 200.0f), "non-square: left of panel");
        check(!r.contains(300.0f, 200.0f), "non-square: right of panel");
        check(!r.contains(250.0f, 400.0f), "non-square: above panel");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
