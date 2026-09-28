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

    // ── toFramebufferRect: identity (scale 1, window at screen origin) ──────
    {
        using noto3d::app::DisplayTransform;
        using noto3d::app::ScreenRect;
        const auto r = noto3d::app::toFramebufferRect(
            ScreenRect{100.0f, 50.0f, 640.0f, 480.0f},
            DisplayTransform{0.0f, 0.0f, 1.0f, 1.0f, 1080});
        // top-left (100,50) + 640x480 → gl_y = 1080 - (50 + 480) = 550
        check(r.x == 100 && r.y == 550 && r.width == 640 && r.height == 480,
              "toFramebufferRect: identity transform");
        check(near(r.aspect(), 640.0f / 480.0f), "toFramebufferRect: identity aspect");
    }

    // ── toFramebufferRect: HiDPI scale 2 (the case that was broken) ──────────
    {
        using noto3d::app::DisplayTransform;
        using noto3d::app::ScreenRect;
        const auto r = noto3d::app::toFramebufferRect(
            ScreenRect{100.0f, 50.0f, 640.0f, 480.0f},
            DisplayTransform{0.0f, 0.0f, 2.0f, 2.0f, 2160});
        // (100,50)*2 = (200,100); size 1280x960; gl_y = 2160 - (100 + 960) = 1100
        check(r.x == 200 && r.y == 1100 && r.width == 1280 && r.height == 960,
              "toFramebufferRect: HiDPI scale 2 maps to framebuffer pixels");
        check(near(r.aspect(), 1280.0f / 960.0f),
              "toFramebufferRect: uniform scale preserves aspect");
    }

    // ── toFramebufferRect: window not at screen origin (second monitor) ─────
    {
        using noto3d::app::DisplayTransform;
        using noto3d::app::ScreenRect;
        const auto r = noto3d::app::toFramebufferRect(
            ScreenRect{100.0f, 50.0f, 640.0f, 480.0f},
            DisplayTransform{1920.0f, 100.0f, 1.0f, 1.0f, 1080});
        // left = 1920 + 100 = 2020; top = 100 + 50 = 150;
        // gl_y = 1080 - (150 + 480) = 450
        check(r.x == 2020 && r.y == 450 && r.width == 640 && r.height == 480,
              "toFramebufferRect: non-zero host viewport origin applied");
    }

    // ── toFramebufferRect: anisotropic scale + origin together ──────────────
    {
        using noto3d::app::DisplayTransform;
        using noto3d::app::ScreenRect;
        const auto r = noto3d::app::toFramebufferRect(
            ScreenRect{200.0f, 100.0f, 400.0f, 300.0f},
            DisplayTransform{100.0f, 50.0f, 2.0f, 1.5f, 1440});
        // left = (100+200)*2 = 600; top = (50+100)*1.5 = 225;
        // size 800x450; gl_y = 1440 - (225 + 450) = 765
        check(r.x == 600 && r.y == 765 && r.width == 800 && r.height == 450,
              "toFramebufferRect: anisotropic scale and origin");
        check(near(r.aspect(), 800.0f / 450.0f),
              "toFramebufferRect: anisotropic aspect");
    }

    // ── toFramebufferRect: rounding of sub-pixel input ──────────────────────
    {
        using noto3d::app::DisplayTransform;
        using noto3d::app::ScreenRect;
        const auto r = noto3d::app::toFramebufferRect(
            ScreenRect{100.5f, 0.0f, 10.0f, 10.0f},
            DisplayTransform{0.0f, 0.0f, 1.0f, 1.0f, 100});
        // 100.5 rounds away from zero → 101; gl_y = 100 - (0 + 10) = 90
        check(r.x == 101 && r.y == 90 && r.width == 10 && r.height == 10,
              "toFramebufferRect: sub-pixel rect rounds to whole framebuffer pixels");
    }

    // ── toFramebufferRect: degenerate inputs ────────────────────────────────
    {
        using noto3d::app::DisplayTransform;
        using noto3d::app::ScreenRect;
        // Minimised window: no framebuffer → invalid, caller skips the 3D pass.
        check(!noto3d::app::toFramebufferRect(
                   ScreenRect{10.0f, 10.0f, 400.0f, 300.0f},
                   DisplayTransform{0.0f, 0.0f, 1.0f, 1.0f, 0}).valid(),
              "toFramebufferRect: framebufferHeight=0 → invalid");
        // Collapsed panel: no content area → invalid.
        check(!noto3d::app::toFramebufferRect(
                   ScreenRect{10.0f, 10.0f, 0.0f, 0.0f},
                   DisplayTransform{0.0f, 0.0f, 1.0f, 1.0f, 1080}).valid(),
              "toFramebufferRect: zero-size panel → invalid");
        // A zero/absent scale must not zero the rect out.
        const auto r = noto3d::app::toFramebufferRect(
            ScreenRect{10.0f, 10.0f, 400.0f, 300.0f},
            DisplayTransform{0.0f, 0.0f, 0.0f, 0.0f, 1080});
        check(r.valid() && r.x == 10 && r.y == 770 && r.width == 400 && r.height == 300,
              "toFramebufferRect: zero scale falls back to 1.0");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
