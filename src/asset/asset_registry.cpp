#include "asset_registry.hpp"
#include "obj_loader.hpp"
#include "log.hpp"

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
        const auto unsafePath = [](const std::string& p) {
            return p.find("../") != std::string::npos ||
                   (!p.empty() && (p[0] == '/' || p[0] == '\\'));
        };
        if (entry.geometry && unsafePath(*entry.geometry)) {
            pr.diagnostics.push_back({entry.id,
                "geometry path must be relative with no '../': " + *entry.geometry,
                LoadStatus::Failed});
            continue;
        }
        if (entry.fallbackGeometry && unsafePath(*entry.fallbackGeometry)) {
            pr.diagnostics.push_back({entry.id,
                "fallback_geometry path must be relative with no '../': " + *entry.fallbackGeometry,
                LoadStatus::Failed});
            continue;
        }

        pr.entries.push_back(std::move(entry));
    }

    return pr;
}

// Upload one set of canonical shapes to GPU → shared renderer::Model
std::shared_ptr<renderer::Model>
uploadToGpu(const std::vector<CanonicalShape>& shapes) {
    auto model = std::make_shared<renderer::Model>();
    model->reserve(shapes.size());
    for (const auto& cs : shapes) {
        renderer::Mesh mesh(cs.vertices, cs.indices);
        model->push_back(renderer::SubMesh{std::move(mesh), cs.material});
    }
    return model;
}

CpuObj toCpuObj(const ObjLoadResult& r) {
    return CpuObj{r.status, r.shapes, r.normalWarnings, r.uvWarnings, r.errorMessage};
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
        auto loadCpu = [&](const std::filesystem::path& p) -> std::shared_ptr<const CpuObj> {
            const std::string key = p.string();
            const auto hit = m_cpuCache.find(key);
            if (hit != m_cpuCache.end()) return hit->second;
            auto cpu = std::make_shared<const CpuObj>(
                toCpuObj(loadObjShapes(p, entry.id, entry.upAxis)));
            if (cpu->status != LoadStatus::Failed) m_cpuCache.emplace(key, cpu);
            return cpu;
        };

        auto       cpu     = loadCpu(canonPath);
        std::string loadKey = canonKey;
        if (cpu->status == LoadStatus::Failed && fallbackPath &&
            fallbackPath->string() != canonKey) {
            const auto fb = loadCpu(*fallbackPath);
            if (fb->status != LoadStatus::Failed) {
                loadKey = fallbackPath->string();
                cpu     = fb;
                m_diagnostics.push_back({entry.id,
                    "geometry failed; explicit fallback_geometry used",
                    LoadStatus::LoadedWithWarnings});
            }
            // fallback also failed → primary failure stands (diagnosed below)
        }

        if (!cpu->errorMessage.empty()) {
            m_diagnostics.push_back({entry.id, cpu->errorMessage, cpu->status});
        }
        if (cpu->normalWarnings > 0) {
            m_diagnostics.push_back({entry.id,
                std::to_string(cpu->normalWarnings) +
                    " vertices had missing normals (area-weighted computed)",
                LoadStatus::LoadedWithWarnings});
        }
        if (cpu->uvWarnings > 0) {
            m_diagnostics.push_back({entry.id,
                std::to_string(cpu->uvWarnings) +
                    " vertices had missing UVs (defaulted to (0,0))",
                LoadStatus::LoadedWithWarnings});
        }

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

        if (mode == Mode::Full) {
            // GPU dedup on the effective path (geometry or fallback actually loaded)
            const auto hit = m_modelCache.find(loadKey);
            if (hit != m_modelCache.end()) {
                rec.model  = hit->second;
                rec.status = cpu->status;
            } else {
                try {
                    auto model            = uploadToGpu(cpu->shapes);
                    m_modelCache.emplace(loadKey, model);
                    rec.model  = std::move(model);
                    rec.status = cpu->status;
                    NOTO3D_DEBUG("[asset] loaded '{}' ({} shape(s))",
                                 entry.id, cpu->shapes.size());
                } catch (const std::exception& e) {
                    m_diagnostics.push_back({entry.id,
                        std::string("GPU upload failed: ") + e.what(), LoadStatus::Failed});
                    rec.status = LoadStatus::Failed;
                }
            }
        } else {
            // ManifestOnly: record CPU-level status, no model
            rec.status = cpu->status;
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
