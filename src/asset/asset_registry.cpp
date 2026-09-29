#include "asset_registry.hpp"
#include "log.hpp"
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
    return doLoad(manifestPath, Mode::Full);
}

RegistryResult AssetRegistry::loadManifestOnly(const std::filesystem::path& manifestPath) {
    return doLoad(manifestPath, Mode::ManifestOnly);
}

RegistryResult AssetRegistry::reload() {
    if (m_manifestPath.empty()) {
        RegistryResult r;
        r.overall = LoadStatus::Failed;
        r.failed  = 1;
        r.diagnostics.push_back({"", "reload() called before load()", LoadStatus::Failed});
        return r;
    }
    return doLoad(m_manifestPath, m_lastMode);
}

RegistryResult AssetRegistry::doLoad(const std::filesystem::path& manifestPath, Mode mode) {
    m_entries.clear();
    m_records.clear();
    m_diagnostics.clear();
    m_cpuCache.clear();
    m_modelCache.clear();
    m_textureCache.clear();
    m_externalCpu.reset();
    m_externalObjDir.clear();
    m_externalCanonKey.clear();
    m_externalModel.reset();
    m_manifestPath = manifestPath;
    m_lastMode     = mode;

    const ManifestParse parsed = parseManifest(manifestPath);
    m_diagnostics.insert(m_diagnostics.end(),
                         parsed.diagnostics.begin(), parsed.diagnostics.end());

    const std::filesystem::path manifestDir = manifestPath.parent_path();

    for (const auto& entry : parsed.entries) {
        m_entries.push_back(entry);
        Record rec;

        if (!entry.geometry) {
            // Geometry-less asset: valid, never a failure (ADR-0007)
            rec.status = LoadStatus::Success;
            m_records.push_back(std::move(rec));
            continue;
        }

        // Canonical absolute path for dedup (ADR-0007)
        std::filesystem::path canonPath;
        try {
            canonPath = std::filesystem::weakly_canonical(manifestDir / *entry.geometry);
        } catch (const std::exception& e) {
            m_diagnostics.push_back({entry.id,
                std::string("path resolution error: ") + e.what(), LoadStatus::Failed});
            m_records.push_back(std::move(rec));
            continue;
        }
        const std::string canonKey = canonPath.string();

        // Fallback candidate path (ADR-0007): explicit opt-in, same safety rules.
        // Only used if the primary geometry fails to load; no fallback system test
        // in M2 (all MVP assets leave it unset).
        std::optional<std::filesystem::path> fallbackPath;
        if (entry.fallbackGeometry) {
            try {
                fallbackPath =
                    std::filesystem::weakly_canonical(manifestDir / *entry.fallbackGeometry);
            } catch (const std::exception& e) {
                m_diagnostics.push_back({entry.id,
                    std::string("fallback path resolution error: ") + e.what(),
                    LoadStatus::LoadedWithWarnings});
            }
        }

        // GPU dedup fast-path: shared immutable Model already uploaded (Full only)
        if (mode == Mode::Full && m_modelCache.count(canonKey)) {
            rec.model  = m_modelCache.at(canonKey);
            rec.status = LoadStatus::Success;
            m_records.push_back(std::move(rec));
            continue;
        }

        // CPU load with dedup by canonical path — shared, immutable, parsed once
        // (noto3d_asset_cpu owns the parse; this registry owns the cache).
        auto        cpu      = m_cpuCache.loadRecord(canonPath, entry.id, entry.upAxis);
        std::string loadKey  = canonKey;
        if (cpu->status == LoadStatus::Failed && fallbackPath &&
            fallbackPath->string() != canonKey) {
            const auto fb = m_cpuCache.loadRecord(*fallbackPath, entry.id, entry.upAxis);
            if (fb->status != LoadStatus::Failed) {
                loadKey = fallbackPath->string();
                cpu     = fb;
                m_diagnostics.push_back({entry.id,
                    "geometry failed; explicit fallback_geometry used",
                    LoadStatus::LoadedWithWarnings});
            }
            // fallback also failed → primary failure stands (diagnosed below)
        }

        appendCpuDiagnostics(m_diagnostics, *cpu, entry.id);

        if (cpu->status == LoadStatus::Failed) {
            rec.status = LoadStatus::Failed;
            m_records.push_back(std::move(rec));
            continue;
        }

        // Defensive: only x/y/z are known axes; unknown → no correction + warning
        if (entry.upAxis != "x" && entry.upAxis != "y" && entry.upAxis != "z") {
            m_diagnostics.push_back({entry.id,
                "unsupported up_axis '" + entry.upAxis +
                    "'; no axis correction applied",
                LoadStatus::LoadedWithWarnings});
        }

        // Ticket 0013: validate diffuseTextureRef paths in the CPU data using the
        // same ADR-0007 predicate applied to manifest geometry paths.  An unsafe
        // ref (absolute or '../') promotes the asset to LOADED_WITH_WARNINGS;
        // the geometry is still uploaded, but the texture is skipped.
        const bool hasUnsafeTexRef = scanUnsafeTextureRefs(m_diagnostics, *cpu, entry.id);

        const LoadStatus baseStatus = hasUnsafeTexRef
            ? LoadStatus::LoadedWithWarnings
            : cpu->status;

        if (mode == Mode::Full) {
            // GPU dedup on the effective path (geometry or fallback actually loaded)
            const auto hit = m_modelCache.find(loadKey);
            if (hit != m_modelCache.end()) {
                rec.model  = hit->second;
                rec.status = baseStatus;
            } else {
                try {
                    auto model = uploadToGpu(
                        cpu->shapes,
                        canonPath.parent_path(),
                        m_textureCache,
                        m_diagnostics,
                        entry.id);
                    m_modelCache.emplace(loadKey, model);
                    rec.model  = std::move(model);
                    rec.status = baseStatus;
                    NOTO3D_DEBUG("[asset] loaded '{}' ({} shape(s))",
                                 entry.id, cpu->shapes.size());
                } catch (const std::exception& e) {
                    m_diagnostics.push_back({entry.id,
                        std::string("GPU upload failed: ") + e.what(), LoadStatus::Failed});
                    rec.status = LoadStatus::Failed;
                }
            }
        } else {
            // ManifestOnly: record CPU-level status (texture safety already checked above)
            rec.status = baseStatus;
        }

        m_records.push_back(std::move(rec));
    }

    return buildResult();
}

RegistryResult AssetRegistry::buildResult() const {
    // The aggregation itself is CPU-only and lives in noto3d_asset_cpu (ADR-0012
    // ②); the only facade-specific part is reading the statuses out of the
    // GPU-shaped records.
    std::vector<LoadStatus> statuses;
    statuses.reserve(m_records.size());
    for (const auto& rec : m_records) statuses.push_back(rec.status);
    return aggregateResults(statuses, m_diagnostics);
}

// ---------------------------------------------------------------------------
// Standalone (manifest-independent) OBJ/MTL loads — ticket 0012 / 0014 fixture
// ---------------------------------------------------------------------------

RegistryResult AssetRegistry::loadModelShapesOnly(const std::filesystem::path& objPath,
                                                  std::string_view             label,
                                                  const std::string&           upAxis) {
    m_externalCpu.reset();
    m_externalObjDir.clear();
    m_externalCanonKey.clear();
    m_externalModel.reset();

    const std::string labelStr(label);
    RegistryResult    r;

    // NOTE: no isUnsafePath() filter at this layer — the call site passes an
    // explicit, trusted OBJ location (e.g. the app's NOTO3D_ASSETS_DIR-derived
    // demo path), not a manifest-controlled field.  ADR-0007 path safety still
    // applies where it protects rooted-relative resolution: MTL diffuseTextureRef
    // refs are checked inside uploadToGpu()/the 0013 scan below (like manifest
    // loads), never bypassing the predicate.

    std::filesystem::path canonPath;
    try {
        canonPath = std::filesystem::weakly_canonical(objPath);
    } catch (const std::exception& e) {
        r.overall = LoadStatus::Failed;
        r.failed  = 1;
        r.diagnostics.push_back({labelStr,
            std::string("path resolution error: ") + e.what(), LoadStatus::Failed});
        m_diagnostics.insert(m_diagnostics.end(), r.diagnostics.begin(), r.diagnostics.end());
        return r;
    }
    const std::string canonKey = canonPath.string();
    m_externalObjDir            = canonPath.parent_path();
    m_externalCanonKey          = canonKey;

    // CPU load with the same canonical-path dedup as manifest loads.
    const auto cpu = m_cpuCache.loadRecord(canonPath, labelStr, upAxis);
    m_externalCpu  = cpu;

    appendCpuDiagnostics(r.diagnostics, *cpu, labelStr);

    if (cpu->status == LoadStatus::Failed) {
        r.overall = LoadStatus::Failed;
        r.failed  = 1;
        m_diagnostics.insert(m_diagnostics.end(), r.diagnostics.begin(), r.diagnostics.end());
        return r;
    }

    // Defensive: only x/y/z are known axes → no correction + warning (mirrors doLoad).
    const bool unsupportedAxis =
        upAxis != "x" && upAxis != "y" && upAxis != "z";
    if (unsupportedAxis) {
        r.diagnostics.push_back({labelStr,
            "unsupported up_axis '" + upAxis + "'; no axis correction applied",
            LoadStatus::LoadedWithWarnings});
    }

    // 0013: same diffuseTextureRef predicate applied to CPU data as manifest loads.
    const bool hasUnsafeTexRef = scanUnsafeTextureRefs(r.diagnostics, *cpu, labelStr);

    // Mirror doLoad()/buildResult(): the tri-state counts record status only, while
    // overall also accounts for diagnostics (incl. the axis warning above).
    r.loaded  = cpu->status == LoadStatus::Success && !hasUnsafeTexRef ? 1 : 0;
    r.warned  = cpu->status == LoadStatus::LoadedWithWarnings || hasUnsafeTexRef ? 1 : 0;
    r.overall = r.warned || unsupportedAxis ? LoadStatus::LoadedWithWarnings
                                            : LoadStatus::Success;

    m_diagnostics.insert(m_diagnostics.end(), r.diagnostics.begin(), r.diagnostics.end());
    return r;
}

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

const renderer::Model* AssetRegistry::externalModel() const noexcept {
    return m_externalModel.get();
}

const AssetEntry* AssetRegistry::findEntry(std::string_view id) const noexcept {
    for (const auto& e : m_entries) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

std::span<const AssetEntry> AssetRegistry::entries() const noexcept {
    return m_entries;
}

const renderer::Model* AssetRegistry::model(std::string_view id) const noexcept {
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            return m_records[i].model.get();
        }
    }
    return nullptr;
}

LoadStatus AssetRegistry::assetStatus(std::string_view id) const noexcept {
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            return m_records[i].status;
        }
    }
    return LoadStatus::Failed;
}

std::span<const LoadDiagnostic> AssetRegistry::diagnostics() const noexcept {
    return m_diagnostics;
}

std::size_t AssetRegistry::uniqueGeometryCount() const noexcept {
    return m_cpuCache.size();
}

}  // namespace noto3d::asset
