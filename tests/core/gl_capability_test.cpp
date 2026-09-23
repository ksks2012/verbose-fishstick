#include "gl_capability.hpp"
#include "log.hpp"

#include <cstdlib>

namespace {

int g_failures = 0;

void expect(bool actual, bool expected, const char* caseName) {
    if (actual == expected) {
        NOTO3D_INFO("PASS  {}", caseName);
    } else {
        NOTO3D_ERROR("FAIL  {} (got {}, want {})", caseName,
                     actual ? "true" : "false", expected ? "true" : "false");
        ++g_failures;
    }
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-tests");

    using noto3d::core::supportsRequiredContext;

    expect(supportsRequiredContext(3, 3, GL_CONTEXT_CORE_PROFILE_BIT), true,
           "supports OpenGL 3.3 core");
    expect(supportsRequiredContext(4, 6, GL_CONTEXT_CORE_PROFILE_BIT), true,
           "supports OpenGL 4.6 core");
    expect(supportsRequiredContext(3, 2, GL_CONTEXT_CORE_PROFILE_BIT), false,
           "rejects OpenGL 3.2 core");
    expect(supportsRequiredContext(3, 3, GL_CONTEXT_COMPATIBILITY_PROFILE_BIT), false,
           "rejects OpenGL 3.3 compatibility profile");

    if (g_failures == 0) {
        NOTO3D_INFO("ALL PASS");
        return EXIT_SUCCESS;
    }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
