// AssetRegistry — the GL-free half (ADR-0012 ③).
//
// This translation unit is compiled into noto3d_asset_cpu, so every definition
// here must reach no GPU symbol: `renderer::Model` and `renderer::Texture` stay
// incomplete (forward-declared in the header) and `uploadToGpu()` is not
// reachable from here at all.  That is what lets tests/asset link the registry's
// CPU contract — loadManifestOnly(), loadModelShapesOnly() and the read-only
// accessors — without linking noto3d_renderer, glad or libGL.
//
// The GPU half (load(), loadModelFile(), reload()) lives in asset_registry.cpp,
// which is compiled into the noto3d_asset facade.  Both halves drive the same
// noto3d_asset_cpu pipeline, so the CPU behaviour of the two modes cannot drift.

#include "asset_registry.hpp"

#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace noto3d::asset {

void AssetRegistry::resetForLoad(const std::filesystem::path& manifestPath, Mode mode) {
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
}

RegistryResult AssetRegistry::loadManifestOnly(const std::filesystem::path& manifestPath) {
    return doLoadManifestOnly(manifestPath);
}

std::vector<CpuEntryLoad> AssetRegistry::runManifestPipeline(
    const std::filesystem::path& manifestPath, const GpuDedupProbe& gpuOwns) {
    const ManifestParse parsed = parseManifest(manifestPath);
    m_diagnostics.insert(m_diagnostics.end(),
                         parsed.diagnostics.begin(), parsed.diagnostics.end());

    auto loads = loadManifestCpu(parsed.entries,
                                 manifestPath.parent_path(),
                                 m_cpuCache,
                                 m_diagnostics,
                                 gpuOwns);

    m_entries = std::move(parsed.entries);
    return loads;
}

RegistryResult AssetRegistry::doLoadManifestOnly(const std::filesystem::path& manifestPath) {
    resetForLoad(manifestPath, Mode::ManifestOnly);

    // No GpuDedupProbe: there is no GPU cache to consult, so every entry is
    // validated on the CPU exactly as Full mode validates the entries it does
    // not find already uploaded.
    const auto loads = runManifestPipeline(manifestPath);

    m_records.reserve(loads.size());
    for (const auto& load : loads) {
        Record rec;
        // model() stays null in ManifestOnly: nothing was uploaded (ADR-0007).
        rec.status = load.status;
        m_records.push_back(std::move(rec));
    }

    return buildResult();
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

    const auto v = validateCpuRecord(*cpu, labelStr, upAxis, r.diagnostics);
    if (v.failed) {
        r.overall = LoadStatus::Failed;
        r.failed  = 1;
        m_diagnostics.insert(m_diagnostics.end(), r.diagnostics.begin(), r.diagnostics.end());
        return r;
    }

    // Parse diagnostics, the axis warning and the 0013 texture scan all ran in
    // validateCpuRecord(), shared with the manifest pipeline.  Only the
    // aggregation below is specific to this path.

    // Mirror the manifest pipeline's aggregation: the tri-state counts record
    // status only, while overall also accounts for diagnostics (incl. the axis
    // warning above).
    r.loaded  = cpu->status == LoadStatus::Success && !v.unsafeTextureRef ? 1 : 0;
    r.warned  = cpu->status == LoadStatus::LoadedWithWarnings || v.unsafeTextureRef ? 1 : 0;
    r.overall = r.warned || v.unsupportedAxis ? LoadStatus::LoadedWithWarnings
                                              : LoadStatus::Success;

    m_diagnostics.insert(m_diagnostics.end(), r.diagnostics.begin(), r.diagnostics.end());
    return r;
}

// ---------------------------------------------------------------------------
// Read-only accessors
// ---------------------------------------------------------------------------

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

RegistryResult AssetRegistry::buildResult() const {
    // The aggregation itself is CPU-only and lives in noto3d_asset_cpu (ADR-0012
    // ②); the only facade-specific part is reading the statuses out of the
    // GPU-shaped records.
    std::vector<LoadStatus> statuses;
    statuses.reserve(m_records.size());
    for (const auto& rec : m_records) statuses.push_back(rec.status);
    return aggregateResults(statuses, m_diagnostics);
}

}  // namespace noto3d::asset
