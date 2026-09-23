#include "gl_capability.hpp"

namespace noto3d::core {

bool supportsRequiredContext(GLint major, GLint minor, GLint profileMask) {
    const bool versionOk = (major > 3) || (major == 3 && minor >= 3);
    const bool coreOk    = (profileMask & GL_CONTEXT_CORE_PROFILE_BIT) != 0;
    return versionOk && coreOk;
}

}  // namespace noto3d::core