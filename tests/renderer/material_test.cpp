#include "material.hpp"
#include "log.hpp"

#include <cmath>
#include <cstdlib>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

bool near(float a, float b) { return std::abs(a - b) < 1e-6f; }

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-renderer-tests");

    using noto3d::renderer::Material;
    using noto3d::renderer::effectiveAlpha;

    // Default material
    {
        const Material m{};
        check(near(effectiveAlpha(m), 1.0f), "default effectiveAlpha == 1.0");
        check(near(m.opacity, 1.0f),         "default opacity == 1.0");
        check(m.cullEnabled,                 "default cullEnabled == true");
        check(near(m.shininess, 32.0f),      "default shininess == 32.0");
        check(!m.texture.has_value(),        "default texture is empty");
    }

    // effectiveAlpha = baseColor.a * opacity  (DEFINITION.md)
    {
        Material m{};
        m.baseColor.a = 0.5f;
        m.opacity     = 0.5f;
        check(near(effectiveAlpha(m), 0.25f), "effectiveAlpha 0.5 * 0.5 = 0.25");
    }
    {
        Material m{};
        m.baseColor.a = 1.0f;
        m.opacity     = 0.0f;
        check(near(effectiveAlpha(m), 0.0f),  "effectiveAlpha 1.0 * 0.0 = 0.0");
    }
    {
        Material m{};
        m.baseColor.a = 0.0f;
        m.opacity     = 1.0f;
        check(near(effectiveAlpha(m), 0.0f),  "effectiveAlpha 0.0 * 1.0 = 0.0");
    }
    {
        Material m{};
        m.baseColor   = glm::vec4(0.8f, 0.6f, 0.4f, 0.8f);
        m.opacity     = 0.75f;
        check(near(effectiveAlpha(m), 0.6f),  "effectiveAlpha 0.8 * 0.75 = 0.6");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
