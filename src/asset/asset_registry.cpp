#include "asset_registry.hpp"
#include "log.hpp"

// The GPU-owned types the upload gate materialises.  asset_registry.hpp only
// forward-declares them so the CPU half stays GL-free (ADR-0012 ③).
#include <material.hpp>
#include <mesh.hpp>
#include <model.hpp>
#include <texture.hpp>

#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace noto3d::asset {

// ---------------------------------------------------------------------------
// AssetRegistry — the GPU facade over the CPU core (ADR-0012 ②).
//
// The CPU half (manifest parse, path predicate, CPU diagnostics, canonical-path
// dedup) lives in noto3d_asset_cpu; this file keeps only the GPU half:
// uploadToGpu() — still the single GPU-upload gate — the GPU dedup caches and
// the aggregation of per-asset status.  One authority, two libraries: the
// registry owns both the CPU cache and the GPU caches (ADR-0007).
// ---------------------------------------------------------------------------

namespace {

// Upload one set of canonical shapes to GPU → shared renderer::Model.
// Resolves diffuseTextureRef paths relative to objDir and uploads textures;
// reference_image paths are NEVER passed here (ADR-0002 structural guarantee).
std::shared_ptr<renderer::Model>
uploadToGpu(const std::vector<CanonicalShape>& shapes,
             const std::filesystem::path& objDir,
             std::unordered_map<std::string,
                                std::shared_ptr<renderer::Texture>>& texCache,
             std::vector<LoadDiagnostic>& diags,
             const std::string& assetId) {
    auto model = std::make_shared<renderer::Model>();
    model->reserve(shapes.size());
    for (const auto& cs : shapes) {
        renderer::Material mat = cs.material;

        // Wire diffuse texture reference → GL texture (ticket 0009)
        // Apply the same ADR-0007 path-safety predicate used for manifest fields
        // (ticket 0013): reject absolute paths and '../' traversal before calling
        // Texture::fromFile().  Rejected refs are cleared on the local Material copy;
        // the geometry still uploads; the asset renders with baseColor only.
        if (mat.diffuseTextureRef) {
            if (isUnsafePath(*mat.diffuseTextureRef)) {
                // Diagnostic was already recorded in doLoad(); clear the local copy.
                mat.diffuseTextureRef.reset();
            }
            std::filesystem::path texPath;
            try {
                texPath = std::filesystem::weakly_canonical(
                    objDir / *mat.diffuseTextureRef);
            } catch (const std::exception& e) {
                diags.push_back({assetId,
                    std::string("texture path error: ") + e.what(),
                    LoadStatus::LoadedWithWarnings});
            }
            if (!texPath.empty()) {
                const std::string key = texPath.string();
                if (!texCache.count(key)) {
                    try {
                        texCache[key] = std::make_shared<renderer::Texture>(
                            renderer::Texture::fromFile(texPath));
                    } catch (const std::exception& e) {
                        diags.push_back({assetId,
                            std::string("texture upload failed: ") + e.what(),
                            LoadStatus::LoadedWithWarnings});
                    }
                }
                if (texCache.count(key)) {
                    mat.texture = texCache.at(key)->id();
                    NOTO3D_DEBUG("[asset] wired texture '{}' → id={} for '{}'",
                                 texPath.filename().string(), mat.texture.value(), assetId);
                }
            }
        }

        renderer::Mesh mesh(cs.vertices, cs.indices);
        model->push_back(renderer::SubMesh{std::move(mesh), std::move(mat)});
    }
    return model;
}

}  // namespace

// ---------------------------------------------------------------------------
// AssetRegistry implementation
// ---------------------------------------------------------------------------

RegistryResult AssetRegistry::load(const std::filesystem::path& manifestPath) {
    return doLoadFull(manifestPath);
}

RegistryResult AssetRegistry::reload() {
    if (m_manifestPath.empty()) {
        RegistryResult r;
        r.overall = LoadStatus::Failed;
        r.failed  = 1;
        r.diagnostics.push_back({"", "reload() called before load()", LoadStatus::Failed});
        return r;
    }
    // Full mode repeats here because only this half owns the upload step;
    // ManifestOnly re-enters the CPU driver's own doLoadManifestOnly().
    if (m_lastMode == Mode::Full) {
        return doLoadFull(m_manifestPath);
    }
    return doLoadManifestOnly(m_manifestPath);
}

RegistryResult AssetRegistry::doLoadFull(const std::filesystem::path& manifestPath) {
    resetForLoad(manifestPath, Mode::Full);

    // The GPU dedup fast path, handed to the CPU pipeline as a probe: when a
    // canonical key is already uploaded, the entry short-circuits to Success with
    // no CPU parse and no diagnostics — the ordering the registry has always had.
    const GpuDedupProbe gpuOwns = [this](std::string_view key) {
        return m_modelCache.find(std::string(key)) != m_modelCache.end();
    };

    // Same driver, same CPU pipeline as ManifestOnly; only the probe differs.
    const auto loads = runManifestPipeline(manifestPath, gpuOwns);

    m_records.reserve(loads.size());
    for (const auto& load : loads) {
        Record rec;
        rec.status = load.status;

        switch (load.origin) {
        case CpuEntryLoad::Origin::GeometryLess:
            // Valid, no geometry, no model (ADR-0007).
            break;
        case CpuEntryLoad::Origin::AlreadyUploaded:
            rec.model = m_modelCache.at(load.cacheKey.string());
            break;
        case CpuEntryLoad::Origin::Cpu:
            if (load.status != LoadStatus::Failed) {
                // GPU dedup on the effective path (geometry or fallback actually
                // loaded).  The probe above means a hit here is only possible for
                // an entry that parsed after the cache was probed.
                if (const auto hit = m_modelCache.find(load.cacheKey.string());
                    hit != m_modelCache.end()) {
                    rec.model = hit->second;
                } else {
                    try {
                        auto model = uploadToGpu(load.cpu->shapes,
                                                 load.textureBaseDir,
                                                 m_textureCache,
                                                 m_diagnostics,
                                                 load.id);
                        m_modelCache.emplace(load.cacheKey.string(), model);
                        rec.model = std::move(model);
                        NOTO3D_DEBUG("[asset] loaded '{}' ({} shape(s))",
                                     load.id, load.cpu->shapes.size());
                    } catch (const std::exception& e) {
                        m_diagnostics.push_back({load.id,
                            std::string("GPU upload failed: ") + e.what(), LoadStatus::Failed});
                        rec.status = LoadStatus::Failed;
                    }
                }
            }
            break;
        }

        m_records.push_back(std::move(rec));
    }

    return buildResult();
}

// ---------------------------------------------------------------------------
// Standalone (manifest-independent) OBJ/MTL → GPU — ticket 0012 / 0014 fixture
// ---------------------------------------------------------------------------
//
// The CPU half, loadModelShapesOnly(), lives in asset_registry_cpu.cpp; this is
// the upload half.  Same public escape hatch, same path predicate, same
// diagnostics — the reference_image exclusion never applies here (ADR-0002).
const renderer::Model* AssetRegistry::loadModelFile(const std::filesystem::path& objPath,
                                                    std::string_view             label,
                                                    const std::string&           upAxis) {
    const RegistryResult cpuResult = loadModelShapesOnly(objPath, label, upAxis);
    if (cpuResult.overall == LoadStatus::Failed || m_externalCpu == nullptr) {
        return nullptr;
    }

    if (const auto hit = m_modelCache.find(m_externalCanonKey); hit != m_modelCache.end()) {
        m_externalModel = hit->second;
    } else {
        try {
            auto model = uploadToGpu(m_externalCpu->shapes,
                                     m_externalObjDir,
                                     m_textureCache,
                                     m_diagnostics,
                                     std::string(label));
            m_modelCache.emplace(m_externalCanonKey, model);
            m_externalModel = std::move(model);
        } catch (const std::exception& e) {
            m_diagnostics.push_back({std::string(label),
                std::string("GPU upload failed: ") + e.what(), LoadStatus::Failed});
            m_externalModel.reset();
            return nullptr;
        }
    }
    return m_externalModel.get();
}

}  // namespace noto3d::asset
