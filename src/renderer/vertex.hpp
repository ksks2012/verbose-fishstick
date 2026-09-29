#pragma once

#include <glm/glm.hpp>

namespace noto3d::renderer {

// ---------------------------------------------------------------------------
// Vertex layout — interleaved, 32 B stride (ADR-0006)
//   location 0 : position  vec3   offset  0
//   location 1 : normal    vec3   offset 12
//   location 2 : uv        vec2   offset 24
//
// Pure data: this header is deliberately GL-free so that a CPU-only consumer
// (e.g. the asset OBJ parse) can name the vertex type without reaching
// glad/gl.h.  The GL-owning half of the vertex contract — the attribute
// locations and the Mesh that uploads this layout — lives in mesh.hpp
// (ADR-0012 ①).
// ---------------------------------------------------------------------------
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};
static_assert(sizeof(Vertex) == 32, "Vertex stride must be 32 B (ADR-0006)");

}  // namespace noto3d::renderer
