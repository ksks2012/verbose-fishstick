#pragma once

#include "manifest_types.hpp"
#include <material.hpp>
#include <mesh.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace noto3d::asset {

// CPU-canonical data for one OBJ shape/material group.
// Never carries GL handles; produced by loadObjShapes() and consumed
// by the registry's GPU-upload step.
struct CanonicalShape {
    std::vector<renderer::Vertex>  vertices;
    std::vector<std::uint32_t>     indices;
    renderer::Material             material;
};

// Result of loading one OBJ file to CPU.
struct ObjLoadResult {
    LoadStatus                  status;
    std::vector<CanonicalShape> shapes;
    int                         normalWarnings = 0;  // vertices where normals were computed
    int                         uvWarnings     = 0;  // vertices where UV defaulted to (0,0)
    std::string                 errorMessage;
};

// Parse an OBJ (+MTL if present) into canonical CPU data.
// No GL calls; safe to call without an active context.
//
//   * Shapes are split by contiguous material runs ("usemtl" regions) into
//     separate CanonicalShapes — each becomes a SubMesh boundary (ADR-0006/0007).
//   * Missing normals → area-weighted vertex normals + count (ADR-0006).
//   * Missing UVs    → canonical (0,0) + count (ADR-0006).
//   * upAxis (default "y") is baked into positions+normals once at load
//     (Y-up pipeline, rotation-only so winding is preserved; ADR-0006/0007).
ObjLoadResult loadObjShapes(const std::filesystem::path& objPath,
                             const std::string&           assetId,
                             const std::string&           upAxis = "y");

}  // namespace noto3d::asset