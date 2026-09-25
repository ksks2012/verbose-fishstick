#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <string>

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

    // Unresolved MTL diffuse texture reference (relative path from the OBJ directory).
    // Recorded at load per ADR-0007; ticket 0009 resolves it into Material::texture.
    std::optional<std::string> diffuseTextureRef;
};

// effectiveAlpha = baseColor.a * opacity  (DEFINITION.md; never silently extended)
[[nodiscard]] inline float effectiveAlpha(const Material& m) noexcept {
    return m.baseColor.a * m.opacity;
}

}  // namespace noto3d::renderer
