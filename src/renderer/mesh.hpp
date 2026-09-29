#pragma once

#include "vertex.hpp"

#include <glad/gl.h>

#include <cstdint>
#include <span>

namespace noto3d::renderer {

// ---------------------------------------------------------------------------
// Vertex attribute locations — fixed contract shared by all shaders (ADR-0006).
// GLSL shaders must use layout(location = N) to match; see shaders/base.vert.
// ---------------------------------------------------------------------------
namespace Attrib {
    constexpr GLuint kPosition = 0;
    constexpr GLuint kNormal   = 1;
    constexpr GLuint kUV       = 2;
}

// ---------------------------------------------------------------------------
// Mesh — immutable GPU resource after upload (ADR-0006)
//
//   * Single interleaved VBO + EBO + VAO; uploaded once in the constructor.
//   * Indices are always GL_UNSIGNED_INT.
//   * Non-copyable; movable (move transfers sole ownership of GPU handles).
//   * A moved-from Mesh is safely destructible (all handles zeroed).
//   * CPU vertex/index arrays are constructor inputs only; no mutation API.
// ---------------------------------------------------------------------------
class Mesh {
public:
    Mesh(std::span<const Vertex> vertices, std::span<const std::uint32_t> indices);
    ~Mesh();

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept;
    Mesh& operator=(Mesh&&) noexcept;

    [[nodiscard]] GLuint  vao()        const noexcept { return m_vao; }
    [[nodiscard]] GLsizei indexCount() const noexcept { return m_indexCount; }

private:
    void release() noexcept;

    GLuint  m_vao        = 0;
    GLuint  m_vbo        = 0;
    GLuint  m_ebo        = 0;
    GLsizei m_indexCount = 0;
};

}  // namespace noto3d::renderer
