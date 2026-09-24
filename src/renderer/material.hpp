#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>

namespace noto3d::renderer {

// Material fields per ADR-0006.
// texture is declared here; binding/sampling deferred to ticket 0009.
struct Material {
    glm::vec4 baseColor   = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // linear RGBA (ADR-0004)
    float     opacity     = 1.0f;
    bool      cullEnabled = true;
    float     shininess   = 32.0f;

    // GL texture name (sRGB diffuse).  Declared per ADR-0006; wired in ticket 0009.
    std::optional<std::uint32_t> texture;
};

// effectiveAlpha = baseColor.a * opacity  (DEFINITION.md; never silently extended)
[[nodiscard]] inline float effectiveAlpha(const Material& m) noexcept {
    return m.baseColor.a * m.opacity;
}

}  // namespace noto3d::renderer
