#include "mesh.hpp"

#include <cstddef>

namespace noto3d::renderer {

Mesh::Mesh(std::span<const Vertex> vertices, std::span<const std::uint32_t> indices)
    : m_indexCount(static_cast<GLsizei>(indices.size())) {

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    // Attribute layout — locations from Attrib constants (ADR-0006); must match shaders/base.vert
    glEnableVertexAttribArray(Attrib::kPosition);
    glVertexAttribPointer(Attrib::kPosition, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, position)));

    glEnableVertexAttribArray(Attrib::kNormal);
    glVertexAttribPointer(Attrib::kNormal, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(Attrib::kUV);
    glVertexAttribPointer(Attrib::kUV, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, uv)));

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    release();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_ebo(other.m_ebo)
    , m_indexCount(other.m_indexCount) {
    other.m_vao        = 0;
    other.m_vbo        = 0;
    other.m_ebo        = 0;
    other.m_indexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        m_vao        = other.m_vao;
        m_vbo        = other.m_vbo;
        m_ebo        = other.m_ebo;
        m_indexCount = other.m_indexCount;
        other.m_vao        = 0;
        other.m_vbo        = 0;
        other.m_ebo        = 0;
        other.m_indexCount = 0;
    }
    return *this;
}

void Mesh::release() noexcept {
    if (m_vao != 0) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo != 0) { glDeleteBuffers(1, &m_vbo);      m_vbo = 0; }
    if (m_ebo != 0) { glDeleteBuffers(1, &m_ebo);      m_ebo = 0; }
    m_indexCount = 0;
}

}  // namespace noto3d::renderer
