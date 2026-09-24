#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "obj_loader.hpp"
#include "log.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace noto3d::asset {

namespace {

// Vertex dedup key: (position_index, normal_index, texcoord_index)
struct VKey {
    int v = -1, n = -1, t = -1;
    bool operator==(const VKey& o) const noexcept {
        return v == o.v && n == o.n && t == o.t;
    }
};
struct VKeyHash {
    std::size_t operator()(const VKey& k) const noexcept {
        std::size_t h = std::hash<int>{}(k.v);
        h ^= std::hash<int>{}(k.n) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.t) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

CanonicalShape buildShape(
    const tinyobj::attrib_t&   attrib,
    const tinyobj::shape_t&    shape,
    const tinyobj::material_t* mat,      // nullptr if no material
    ObjLoadResult&             out)
{
    CanonicalShape cs;

    // Material from MTL (ADR-0006; texture resolved in ticket 0009)
    if (mat) {
        cs.material.baseColor = glm::vec4(mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], 1.0f);
        cs.material.shininess = (mat->shininess > 0.0f) ? mat->shininess : 32.0f;
        cs.material.opacity   = mat->dissolve;
        if (!mat->diffuse_texname.empty()) {
            NOTO3D_DEBUG("[asset] texture ref '{}' deferred to ticket 0009", mat->diffuse_texname);
        }
    }

    std::unordered_map<VKey, std::uint32_t, VKeyHash> vertexMap;
    std::vector<bool> needsNormal;
    int uvWarn = 0;

    for (const auto& idx : shape.mesh.indices) {
        VKey key{idx.vertex_index, idx.normal_index, idx.texcoord_index};
        auto it = vertexMap.find(key);
        if (it == vertexMap.end()) {
            renderer::Vertex v;

            const auto vi = static_cast<std::size_t>(idx.vertex_index);
            v.position = glm::vec3(attrib.vertices[3 * vi],
                                   attrib.vertices[3 * vi + 1],
                                   attrib.vertices[3 * vi + 2]);

            if (idx.normal_index >= 0) {
                const auto ni = static_cast<std::size_t>(idx.normal_index);
                v.normal = glm::vec3(attrib.normals[3 * ni],
                                     attrib.normals[3 * ni + 1],
                                     attrib.normals[3 * ni + 2]);
                needsNormal.push_back(false);
            } else {
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);  // placeholder; replaced below
                needsNormal.push_back(true);
            }

            if (idx.texcoord_index >= 0) {
                const auto ti = static_cast<std::size_t>(idx.texcoord_index);
                v.uv = glm::vec2(attrib.texcoords[2 * ti],
                                 attrib.texcoords[2 * ti + 1]);
            } else {
                v.uv = glm::vec2(0.0f, 0.0f);  // canonical (0,0) per ADR-0006
                ++uvWarn;
            }

            const auto newIdx = static_cast<std::uint32_t>(cs.vertices.size());
            it = vertexMap.emplace(key, newIdx).first;
            cs.vertices.push_back(v);
        }
        cs.indices.push_back(it->second);
    }

    // Area-weighted normal computation for vertices with no OBJ normal (ADR-0006)
    const bool anyMissing = [&] {
        for (bool b : needsNormal) { if (b) return true; }
        return false;
    }();

    if (anyMissing) {
        std::vector<glm::vec3> accum(cs.vertices.size(), glm::vec3(0.0f));

        for (std::size_t i = 0; i + 2 < cs.indices.size(); i += 3) {
            const auto i0 = cs.indices[i], i1 = cs.indices[i + 1], i2 = cs.indices[i + 2];
            const glm::vec3 fn = glm::cross(cs.vertices[i1].position - cs.vertices[i0].position,
                                            cs.vertices[i2].position - cs.vertices[i0].position);
            if (i0 < needsNormal.size() && needsNormal[i0]) accum[i0] += fn;
            if (i1 < needsNormal.size() && needsNormal[i1]) accum[i1] += fn;
            if (i2 < needsNormal.size() && needsNormal[i2]) accum[i2] += fn;
        }

        int nWarn = 0;
        for (std::size_t i = 0; i < cs.vertices.size(); ++i) {
            if (i < needsNormal.size() && needsNormal[i]) {
                const float len = glm::length(accum[i]);
                cs.vertices[i].normal = (len > 1e-6f)
                    ? accum[i] / len
                    : glm::vec3(0.0f, 1.0f, 0.0f);
                ++nWarn;
            }
        }
        out.normalWarnings += nWarn;
    }

    out.uvWarnings += uvWarn;
    return cs;
}

}  // namespace

ObjLoadResult loadObjShapes(const std::filesystem::path& objPath,
                             const std::string&           assetId) {
    ObjLoadResult result;
    result.status = LoadStatus::Success;

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    const bool ok = tinyobj::LoadObj(
        &attrib, &shapes, &materials, &err,
        objPath.string().c_str(),
        objPath.parent_path().string().c_str());

    if (!err.empty()) {
        NOTO3D_WARN("[asset] OBJ messages for {}: {}", assetId, err);
    }
    if (!ok) {
        result.status = LoadStatus::Failed;
        result.errorMessage = "[obj] " + assetId + ": " +
                              (err.empty() ? "unknown error" : err);
        return result;
    }
    if (shapes.empty()) {
        result.status       = LoadStatus::Failed;
        result.errorMessage = "[obj] " + assetId + ": no shapes in file";
        return result;
    }

    for (const auto& shape : shapes) {
        int matId = -1;
        if (!shape.mesh.material_ids.empty()) {
            matId = shape.mesh.material_ids[0];
        }
        const tinyobj::material_t* mat =
            (matId >= 0 && matId < static_cast<int>(materials.size()))
            ? &materials[static_cast<std::size_t>(matId)]
            : nullptr;

        result.shapes.push_back(buildShape(attrib, shape, mat, result));
    }

    if (result.normalWarnings > 0 || result.uvWarnings > 0) {
        result.status = LoadStatus::LoadedWithWarnings;
    }
    return result;
}

}  // namespace noto3d::asset
