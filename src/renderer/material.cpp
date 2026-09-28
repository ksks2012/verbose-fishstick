#include "material.hpp"

namespace noto3d::renderer {

float effectiveAlpha(const Material& m) noexcept {
    return m.baseColor.a * m.opacity;
}

}  // namespace noto3d::renderer
