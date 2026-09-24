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

        // Path safety: no absolute paths, no "../" (ADR-0007)
        if (entry.geometry) {
            const std::string& gp = *entry.geometry;
            if (gp.find("../") != std::string::npos ||
                (!gp.empty() && (gp[0] == '/' || gp[0] == '\\'))) {
                pr.diagnostics.push_back({entry.id,
                    "geometry path must be relative with no '../': " + gp,
                    LoadStatus::Failed});
                continue;
            }
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

        // Dedup: if already loaded (GPU or CPU), reuse
        if (mode == Mode::Full && m_modelCache.count(canonKey)) {
            rec.model  = m_modelCache.at(canonKey);
            rec.status = LoadStatus::Success;
            m_records.push_back(std::move(rec));
            continue;
        }

        // Load OBJ to CPU (no GL)
        ObjLoadResult obj = loadObjShapes(canonPath, entry.id);

        if (!obj.errorMessage.empty()) {
            m_diagnostics.push_back({entry.id, obj.errorMessage, obj.status});
        }
        if (obj.normalWarnings > 0) {
            m_diagnostics.push_back({entry.id,
                std::to_string(obj.normalWarnings) +
                    " vertices had missing normals (area-weighted computed)",
                LoadStatus::LoadedWithWarnings});
        }
        if (obj.uvWarnings > 0) {
            m_diagnostics.push_back({entry.id,
                std::to_string(obj.uvWarnings) +
                    " vertices had missing UVs (defaulted to (0,0))",
                LoadStatus::LoadedWithWarnings});
        }

        if (obj.status == LoadStatus::Failed) {
            rec.status = LoadStatus::Failed;
            m_records.push_back(std::move(rec));
            continue;
        }

        if (mode == Mode::Full) {
            try {
                auto model        = uploadToGpu(obj.shapes);
                m_modelCache[canonKey] = model;
                rec.model  = std::move(model);
                rec.status = obj.status;
                NOTO3D_DEBUG("[asset] loaded '{}' ({} shape(s))", entry.id, obj.shapes.size());
            } catch (const std::exception& e) {
                m_diagnostics.push_back({entry.id,
                    std::string("GPU upload failed: ") + e.what(), LoadStatus::Failed});
                rec.status = LoadStatus::Failed;
            }
        } else {
            // ManifestOnly: record CPU-level status, no model
            rec.status = obj.status;
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

}  // namespace noto3d::asset
