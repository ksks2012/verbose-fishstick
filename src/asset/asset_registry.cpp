#include "asset_registry.hpp"
#include "obj_loader.hpp"
#include "log.hpp"
#include <texture.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <filesystem>
#include <string>

namespace noto3d::asset {

// ---------------------------------------------------------------------------
// Manifest parsing (pure CPU, no GL)
// ---------------------------------------------------------------------------

namespace {

std::optional<GeometrySource> parseGeoSource(const std::string& s) {
    if (s == "placeholder") return GeometrySource::Placeholder;
    if (s == "official")    return GeometrySource::Official;
    return std::nullopt;
}

// Shared ADR-0007 path-safety predicate (ticket 0013).
// Returns true when a path is unsafe for asset loading:
//   * absolute (starts with '/' or '\')
//   * contains '../' traversal sequence
// Used for manifest geometry/fallback_geometry fields AND diffuseTextureRef.
bool isUnsafePath(const std::string& p) {
    return p.find("../") != std::string::npos ||
           (!p.empty() && (p[0] == '/' || p[0] == '\\'));
}

struct ParseResult {
    std::vector<AssetEntry>     entries;
    std::vector<LoadDiagnostic> diagnostics;
};

ParseResult parseManifest(const std::filesystem::path& manifestPath) {
    ParseResult pr;

    std::ifstream f(manifestPath);
    if (!f) {
        pr.diagnostics.push_back({"", "cannot open manifest: " + manifestPath.string(),
                                  LoadStatus::Failed});
        return pr;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(f);
    } catch (const nlohmann::json::exception& e) {
        pr.diagnostics.push_back({"", std::string("manifest JSON error: ") + e.what(),
                                  LoadStatus::Failed});
        return pr;
    }

    if (!j.contains("assets") || !j["assets"].is_array()) {
        pr.diagnostics.push_back({"", "manifest missing 'assets' array",
                                  LoadStatus::Failed});
        return pr;
    }

    for (const auto& a : j["assets"]) {
        // id is required
        if (!a.contains("id") || !a["id"].is_string()) {
            pr.diagnostics.push_back({"", "asset entry missing 'id'", LoadStatus::Failed});
            continue;
        }

        AssetEntry entry;
        entry.id = a["id"].get<std::string>();

        if (a.contains("reference_image") && a["reference_image"].is_string()) {
            entry.referenceImage = a["reference_image"].get<std::string>();
        }
        if (a.contains("geometry") && !a["geometry"].is_null()) {
            entry.geometry = a["geometry"].get<std::string>();
        }
        if (a.contains("geometry_source") && !a["geometry_source"].is_null()) {
            const std::string srcStr = a["geometry_source"].get<std::string>();
            entry.geometrySource = parseGeoSource(srcStr);
            if (!entry.geometrySource) {
                pr.diagnostics.push_back({entry.id,
                    "unknown geometry_source: '" + srcStr + "'", LoadStatus::Failed});
                continue;
            }
        }

        // Invariant: geometry_source non-null iff geometry non-null (ADR-0007)
        if (entry.geometry.has_value() != entry.geometrySource.has_value()) {
            pr.diagnostics.push_back({entry.id,
                "invariant violated: geometry and geometry_source must both be null or both set",
                LoadStatus::Failed});
            continue;
        }

        if (a.contains("fallback_geometry") && !a["fallback_geometry"].is_null()) {
            entry.fallbackGeometry = a["fallback_geometry"].get<std::string>();
        }

        if (a.contains("axis_data") && a["axis_data"].is_object()) {
            const auto& ax = a["axis_data"];
            if (ax.contains("up_axis") && ax["up_axis"].is_string()) {
                entry.upAxis = ax["up_axis"].get<std::string>();
            }
            if (ax.contains("recorded_winding") && !ax["recorded_winding"].is_null()) {
                entry.recordedWinding = ax["recorded_winding"].get<std::string>();
            }
        }

        // Path safety: no absolute paths, no "../" (ADR-0007) — geometry and
        // fallback_geometry are checked alike.
        if (entry.geometry && isUnsafePath(*entry.geometry)) {
            pr.diagnostics.push_back({entry.id,
                "geometry path must be relative with no '../': " + *entry.geometry,
                LoadStatus::Failed});
            continue;
        }
        if (entry.fallbackGeometry && isUnsafePath(*entry.fallbackGeometry)) {
            pr.diagnostics.push_back({entry.id,
                "fallback_geometry path must be relative with no '../': " + *entry.fallbackGeometry,
                LoadStatus::Failed});
            continue;
        }

        pr.entries.push_back(std::move(entry));
    }

    return pr;
}

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

CpuObj toCpuObj(const ObjLoadResult& r) {
    return CpuObj{r.status, r.shapes, r.normalWarnings, r.uvWarnings, r.errorMessage};
}

// Push the parse-recorded diagnostics (errorMessage / normalWarnings / uvWarnings) for a CPU
// result.  Used identically by manifest loads and standalone loads.
void appendCpuDiagnostics(std::vector<LoadDiagnostic>& out, const CpuObj& cpu,
                          const std::string& label) {
    if (!cpu.errorMessage.empty()) {
        out.push_back({label, cpu.errorMessage, cpu.status});
    }
    if (cpu.normalWarnings > 0) {
        out.push_back({label,
            std::to_string(cpu.normalWarnings) +
                " vertices had missing normals (area-weighted computed)",
            LoadStatus::LoadedWithWarnings});
    }
    if (cpu.uvWarnings > 0) {
        out.push_back({label,
            std::to_string(cpu.uvWarnings) + " vertices had missing UVs (defaulted to (0,0))",
            LoadStatus::LoadedWithWarnings});
    }
}

// Ticket 0013: reject diffuseTextureRef paths that violate the ADR-0007 predicate
// (absolute or '../').  Each rejection is recorded as a warning diagnostic.
bool scanUnsafeTextureRefs(std::vector<LoadDiagnostic>& out, const CpuObj& cpu,
                           const std::string& label) {
    bool hasUnsafe = false;
    for (const auto& shape : cpu.shapes) {
        if (shape.material.diffuseTextureRef &&
            isUnsafePath(*shape.material.diffuseTextureRef)) {
            out.push_back({label,
                "texture ref '" + *shape.material.diffuseTextureRef +
                    "' rejected: absolute paths and '../' not allowed (ADR-0007)",
                LoadStatus::LoadedWithWarnings});
            hasUnsafe = true;
        }
    }
    return hasUnsafe;
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

    const auto [entries, parseDiags] = parseManifest(manifestPath);
    m_diagnostics.insert(m_diagnostics.end(), parseDiags.begin(), parseDiags.end());

    const std::filesystem::path manifestDir = manifestPath.parent_path();

    for (const auto& entry : entries) {
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

        // CPU load with dedup by canonical path — shared, immutable, parsed once.
        auto       cpu      = loadCpuRecord(canonPath, entry.id, entry.upAxis);
        std::string loadKey = canonKey;
        if (cpu->status == LoadStatus::Failed && fallbackPath &&
            fallbackPath->string() != canonKey) {
            const auto fb = loadCpuRecord(*fallbackPath, entry.id, entry.upAxis);
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
    RegistryResult r;
    r.diagnostics = m_diagnostics;

    for (const auto& rec : m_records) {
        switch (rec.status) {
            case LoadStatus::Success:            ++r.loaded; break;
            case LoadStatus::LoadedWithWarnings: ++r.warned; break;
            case LoadStatus::Failed:             ++r.failed; break;
        }
    }

    bool hasFailed   = (r.failed > 0);
    bool hasWarnings = (r.warned > 0);
    for (const auto& d : m_diagnostics) {
        if (d.severity == LoadStatus::Failed)             hasFailed   = true;
        if (d.severity == LoadStatus::LoadedWithWarnings) hasWarnings = true;
    }

    if (hasFailed)        r.overall = LoadStatus::Failed;
    else if (hasWarnings) r.overall = LoadStatus::LoadedWithWarnings;
    else                  r.overall = LoadStatus::Success;

    return r;
}

std::shared_ptr<const CpuObj> AssetRegistry::loadCpuRecord(const std::filesystem::path& path,
                                                           const std::string&           label,
                                                           const std::string&           upAxis) {
    const std::string key = path.string();
    if (const auto hit = m_cpuCache.find(key); hit != m_cpuCache.end()) return hit->second;
    auto cpu = std::make_shared<const CpuObj>(toCpuObj(loadObjShapes(path, label, upAxis)));
    if (cpu->status != LoadStatus::Failed) m_cpuCache.emplace(key, cpu);
    return cpu;
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
    const auto cpu = loadCpuRecord(canonPath, labelStr, upAxis);
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
