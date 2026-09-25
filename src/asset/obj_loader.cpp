#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "obj_loader.hpp"
#include "log.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <string>
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

// Faces [faceBegin, faceEnd) of `shape` → one canonical CPU shape.
CanonicalShape buildShapeFromFaces(
    const tinyobj::attrib_t&   attrib,
    const tinyobj::shape_t&    shape,
    std::size_t                faceBegin,
    std::size_t                faceEnd,
    const tinyobj::material_t* mat,          // nullptr → default material
    ObjLoadResult&             out)
{
    CanonicalShape cs;

    // Material from MTL (ADR-0006; texture resolved in ticket 0009)
    if (mat) {
        cs.material.baseColor = glm::vec4(mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], 1.0f);
        cs.material.shininess = (mat->shininess > 0.0f) ? mat->shininess : 32.0f;
        cs.material.opacity   = mat->dissolve;
        if (!mat->diffuse_texname.empty()) {
            cs.material.diffuseTextureRef = mat->diffuse_texname;
            NOTO3D_DEBUG("[asset] texture ref '{}' recorded; deferred to ticket 0009",
                         mat->diffuse_texname);
        }
    }

    std::unordered_map<VKey, std::uint32_t, VKeyHash> vertexMap;
    std::vector<bool> needsNormal;
    int uvWarn = 0;

    for (std::size_t fi = faceBegin; fi < faceEnd; ++fi) {
        for (int k = 0; k < 3; ++k) {
            const auto& idx = shape.mesh.indices[fi * 3 + k];
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

int matIdAt(const tinyobj::shape_t& shape, std::size_t face) {
    if (shape.mesh.material_ids.empty()) return -1;
    if (face >= shape.mesh.material_ids.size()) return -1;
    return shape.mesh.material_ids[face];
}

// Split one tinyobj shape into canonical shapes by contiguous material runs —
// each "usemtl" region becomes a SubMesh boundary (ADR-0006/0007).
void splitShapeByMaterial(
    const tinyobj::attrib_t&             attrib,
    const tinyobj::shape_t&              shape,
    const std::vector<tinyobj::material_t>& materials,
    ObjLoadResult&                       out)
{
    const std::size_t faceCount = shape.mesh.indices.size() / 3;
    if (faceCount == 0) return;

    std::size_t runBegin = 0;
    while (runBegin < faceCount) {
        const int firstMat = matIdAt(shape, runBegin);
        std::size_t runEnd = runBegin + 1;
        while (runEnd < faceCount && matIdAt(shape, runEnd) == firstMat) {
            ++runEnd;
        }

        const tinyobj::material_t* mat = nullptr;
        if (firstMat >= 0 && firstMat < static_cast<int>(materials.size())) {
            mat = &materials[static_cast<std::size_t>(firstMat)];
        }

        out.shapes.push_back(
            buildShapeFromFaces(attrib, shape, runBegin, runEnd, mat, out));
        runBegin = runEnd;
    }
}

// Rotation-only Y-up correction: source axis → +Y (ADR-0006/0007, baked once).
// Pure rotation is orientation-preserving, so winding is left intact.
glm::mat3 upAxisCorrection(const std::string& upAxis) {
    if (upAxis == "z") {
        // Rx(-90°): (x,y,z) → (x,z,-y);  source +Z → +Y
        return glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
                                     glm::vec3(1, 0, 0)));
    }
    if (upAxis == "x") {
        // Rz(+90°): (x,y,z) → (-y,x,z);  source +X → +Y
        return glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                                     glm::vec3(0, 0, 1)));
    }
    return glm::mat3(1.0f);  // "y" or unknown → identity (no-op)
}

void applyAxisCorrection(const std::string& upAxis,
                         std::vector<CanonicalShape>& shapes) {
    if (upAxis != "x" && upAxis != "z") return;
    const glm::mat3 rot = upAxisCorrection(upAxis);
    NOTO3D_DEBUG("[asset] baked axis correction: up_axis='{}'", upAxis);
    for (auto& s : shapes) {
        for (auto& v : s.vertices) {
            v.position = rot * v.position;
            v.normal   = rot * v.normal;
        }
    }
}

}  // namespace

ObjLoadResult loadObjShapes(const std::filesystem::path& objPath,
                            const std::string&           assetId,
                            const std::string&           upAxis) {
    ObjLoadResult result;
    result.status = LoadStatus::Success;

    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    // tinyobj concatenates the base dir with the mtllib name verbatim
    // (m_mtlBaseDir + matId, no separator) — ensure a trailing slash.
    std::string mtlBase = objPath.parent_path().string();
    if (!mtlBase.empty() && mtlBase.back() != '/') {
        mtlBase.push_back('/');
    }

    const bool ok = tinyobj::LoadObj(
        &attrib, &shapes, &materials, &err,
        objPath.string().c_str(),
        mtlBase.c_str());

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
        splitShapeByMaterial(attrib, shape, materials, result);
    }

    // Y-up axis correction baked once into the canonical CPU data (ADR-0006/0007)
    applyAxisCorrection(upAxis, result.shapes);

    if (result.normalWarnings > 0 || result.uvWarnings > 0) {
        result.status = LoadStatus::LoadedWithWarnings;
    }
    return result;
}

}  // namespace noto3d::asset