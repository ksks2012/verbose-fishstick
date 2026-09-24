#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <span>

namespace noto3d::renderer {

// ---------------------------------------------------------------------------
// Vertex layout — interleaved, 32 B stride (ADR-0006)
//   location 0 : position  vec3   offset  0
//   location 1 : normal    vec3   offset 12
//   location 2 : uv        vec2   offset 24
// ---------------------------------------------------------------------------
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};
static_assert(sizeof(Vertex) == 32, "Vertex stride must be 32 B (ADR-0006)");

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
