#pragma once

#include "mesh.hpp"
#include "material.hpp"

#include <vector>

namespace noto3d::renderer {

// SubMesh = one (Mesh, Material) pair.
// Non-copyable (Mesh is non-copyable), movable.
struct SubMesh {
    Mesh     mesh;
    Material material;
};

// Model = ordered list of SubMeshes (OBJ/MTL may carry multiple materials).
using Model = std::vector<SubMesh>;

}  // namespace noto3d::renderer
