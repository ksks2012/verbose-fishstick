#pragma once

#include <glad/gl.h>

namespace noto3d::core {

// Dependency-free capability check (ADR-0004, M0 ticket): decides whether a numeric
// OpenGL context version + profile satisfies the 3.3 Core baseline. Never parses the
// GL_VERSION string. Testable without fabricating a real sub-3.3 context.
bool supportsRequiredContext(GLint major, GLint minor, GLint profileMask);

}  // namespace noto3d::core