#include "asset_cpu.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <exception>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

namespace noto3d::asset {

// ---------------------------------------------------------------------------
// Asset CPU core (ADR-0012 ②): manifest parsing, the ADR-0007 path predicate,
// CPU diagnostics and canonical-path deduplication.  No GL, no renderer types
// beyond the GL-free interchange structs (Vertex, Material).
// ---------------------------------------------------------------------------

namespace {

std::optional<GeometrySource> parseGeoSource(const std::string& s) {
    if (s == "placeholder") return GeometrySource::Placeholder;
    if (s == "official")    return GeometrySource::Official;
    return std::nullopt;
}

CpuObj toCpuObj(const ObjLoadResult& r) {
    return CpuObj{.status          = r.status,
                 .shapes          = r.shapes,
                 .normalWarnings  = r.normalWarnings,
                 .uvWarnings      = r.uvWarnings,
                 .errorMessage    = r.errorMessage};
}

}  // namespace

bool isUnsafePath(std::string_view p) noexcept {
    return p.find("../") != std::string_view::npos ||
           (!p.empty() && (p[0] == '/' || p[0] == '\\'));
}

ManifestParse parseManifest(const std::filesystem::path& manifestPath) {
    ManifestParse parse;

    std::ifstream f(manifestPath);
    if (!f) {
        parse.diagnostics.push_back({"", "cannot open manifest: " + manifestPath.string(),
                                     LoadStatus::Failed});
        return parse;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(f);
    } catch (const nlohmann::json::exception& e) {
        parse.diagnostics.push_back({"", std::string("manifest JSON error: ") + e.what(),
                                     LoadStatus::Failed});
        return parse;
    }

    if (!j.contains("assets") || !j["assets"].is_array()) {
        parse.diagnostics.push_back({"", "manifest missing 'assets' array",
                                     LoadStatus::Failed});
        return parse;
    }

    for (const auto& a : j["assets"]) {
        // id is required
        if (!a.contains("id") || !a["id"].is_string()) {
            parse.diagnostics.push_back({"", "asset entry missing 'id'", LoadStatus::Failed});
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
                parse.diagnostics.push_back({entry.id,
                    "unknown geometry_source: '" + srcStr + "'", LoadStatus::Failed});
                continue;
            }
        }

        // Invariant: geometry_source non-null iff geometry non-null (ADR-0007)
        if (entry.geometry.has_value() != entry.geometrySource.has_value()) {
            parse.diagnostics.push_back({entry.id,
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
            parse.diagnostics.push_back({entry.id,
                "geometry path must be relative with no '../': " + *entry.geometry,
                LoadStatus::Failed});
            continue;
        }
        if (entry.fallbackGeometry && isUnsafePath(*entry.fallbackGeometry)) {
            parse.diagnostics.push_back({entry.id,
                "fallback_geometry path must be relative with no '../': " + *entry.fallbackGeometry,
                LoadStatus::Failed});
            continue;
        }

        parse.entries.push_back(std::move(entry));
    }

    return parse;
}

void appendCpuDiagnostics(std::vector<LoadDiagnostic>& out,
                          const CpuObj&                 cpu,
                          std::string_view               label) {
    if (!cpu.errorMessage.empty()) {
        out.push_back({std::string(label), cpu.errorMessage, cpu.status});
    }
    if (cpu.normalWarnings > 0) {
        out.push_back({std::string(label),
            std::to_string(cpu.normalWarnings) +
                " vertices had missing normals (area-weighted computed)",
            LoadStatus::LoadedWithWarnings});
    }
    if (cpu.uvWarnings > 0) {
        out.push_back({std::string(label),
            std::to_string(cpu.uvWarnings) + " vertices had missing UVs (defaulted to (0,0))",
            LoadStatus::LoadedWithWarnings});
    }
}

bool scanUnsafeTextureRefs(std::vector<LoadDiagnostic>& out,
                           const CpuObj&                 cpu,
                           std::string_view               label) {
    bool hasUnsafe = false;
    for (const auto& shape : cpu.shapes) {
        if (shape.material.diffuseTextureRef &&
            isUnsafePath(*shape.material.diffuseTextureRef)) {
            out.push_back({std::string(label),
                "texture ref '" + *shape.material.diffuseTextureRef +
                    "' rejected: absolute paths and '../' not allowed (ADR-0007)",
                LoadStatus::LoadedWithWarnings});
            hasUnsafe = true;
        }
    }
    return hasUnsafe;
}

// The shared per-entry epilogue — see validateCpuRecord() in the header.
CpuValidation validateCpuRecord(const CpuObj&               cpu,
                                std::string_view             label,
                                const std::string&           upAxis,
                                std::vector<LoadDiagnostic>& out) {
    appendCpuDiagnostics(out, cpu, label);

    CpuValidation v;
    if (cpu.status == LoadStatus::Failed) {
        // A failed parse is diagnosed and nothing else applies to it — the same
        // point at which the pipeline stopped before the rules were shared.
        v.failed = true;
        return v;
    }

    // Defensive: only x/y/z are known axes; unknown → no correction + warning
    if (upAxis != "x" && upAxis != "y" && upAxis != "z") {
        v.unsupportedAxis = true;
        out.push_back({std::string(label),
            "unsupported up_axis '" + upAxis + "'; no axis correction applied",
            LoadStatus::LoadedWithWarnings});
    }

    // Ticket 0013: validate diffuseTextureRef paths in the CPU data using the
    // same ADR-0007 predicate applied to manifest geometry paths.  An unsafe ref
    // (absolute or '../') promotes the asset to LOADED_WITH_WARNINGS; the
    // geometry is still uploaded, but the texture is skipped.
    v.unsafeTextureRef = scanUnsafeTextureRefs(out, cpu, label);
    return v;
}

// ---------------------------------------------------------------------------
// RegistryResult aggregation
// ---------------------------------------------------------------------------

RegistryResult aggregateResults(std::span<const LoadStatus>     statuses,
                                std::span<const LoadDiagnostic> diagnostics) {
    RegistryResult r;
    r.diagnostics.assign(diagnostics.begin(), diagnostics.end());

    for (const LoadStatus status : statuses) {
        switch (status) {
            case LoadStatus::Success:            ++r.loaded; break;
            case LoadStatus::LoadedWithWarnings: ++r.warned; break;
            case LoadStatus::Failed:             ++r.failed; break;
        }
    }

    // `overall` accounts for the counts AND the diagnostic severities, so a
    // warning that never reached a record still degrades the overall status.
    bool hasFailed   = (r.failed > 0);
    bool hasWarnings = (r.warned > 0);
    for (const auto& d : r.diagnostics) {
        if (d.severity == LoadStatus::Failed)             hasFailed   = true;
        if (d.severity == LoadStatus::LoadedWithWarnings) hasWarnings = true;
    }

    if (hasFailed)        r.overall = LoadStatus::Failed;
    else if (hasWarnings) r.overall = LoadStatus::LoadedWithWarnings;
    else                  r.overall = LoadStatus::Success;

    return r;
}

// ---------------------------------------------------------------------------
// CpuCache — parse once, share the immutable record (ADR-0007)
// ---------------------------------------------------------------------------

std::shared_ptr<const CpuObj> CpuCache::loadRecord(const std::filesystem::path& path,
                                                   std::string_view               label,
                                                   const std::string&             upAxis) {
    const std::string key = path.string();
    if (const auto hit = m_records.find(key); hit != m_records.end()) return hit->second;
    auto cpu = std::make_shared<const CpuObj>(
        toCpuObj(loadObjShapes(path, std::string(label), upAxis)));
    if (cpu->status != LoadStatus::Failed) m_records.emplace(key, cpu);
    return cpu;
}

void CpuCache::clear() noexcept {
    m_records.clear();
}

std::size_t CpuCache::size() const noexcept {
    return m_records.size();
}

// ---------------------------------------------------------------------------
// The manifest CPU pipeline
// ---------------------------------------------------------------------------

std::vector<CpuEntryLoad> loadManifestCpu(std::span<const AssetEntry>  entries,
                                          const std::filesystem::path& manifestDir,
                                          CpuCache&                    cache,
                                          std::vector<LoadDiagnostic>& diagnostics,
                                          const GpuDedupProbe&         gpuOwns) {
    using Origin = CpuEntryLoad::Origin;

    std::vector<CpuEntryLoad> out;
    out.reserve(entries.size());

    for (const auto& entry : entries) {
        CpuEntryLoad rec;
        rec.id = entry.id;

        if (!entry.geometry) {
            // Geometry-less asset: valid, never a failure (ADR-0007)
            rec.origin = Origin::GeometryLess;
            rec.status = LoadStatus::Success;
            out.push_back(std::move(rec));
            continue;
        }

        // Canonical absolute path for dedup (ADR-0007)
        std::filesystem::path canonPath;
        try {
            canonPath = std::filesystem::weakly_canonical(manifestDir / *entry.geometry);
        } catch (const std::exception& e) {
            diagnostics.push_back({entry.id,
                std::string("path resolution error: ") + e.what(), LoadStatus::Failed});
            out.push_back(std::move(rec));  // status stays Failed
            continue;
        }
        const std::string canonKey = canonPath.string();
        rec.textureBaseDir        = canonPath.parent_path();

        // Fallback candidate path (ADR-0007): explicit opt-in, same safety rules.
        // Only used if the primary geometry fails to load; no fallback system test
        // in M2 (all MVP assets leave it unset).
        //
        // Resolved BEFORE the GPU dedup probe, matching the pre-split order: an
        // unresolvable fallback_geometry is a per-entry warning in its own right
        // and is reported even when the entry then short-circuits to an
        // already-uploaded model.  Moving this below the probe would silently
        // drop that diagnostic (and stop degrading `overall`) for exactly those
        // entries — reachable only by a hand-written manifest, which is why no
        // test covered it until the review.
        std::optional<std::filesystem::path> fallbackPath;
        if (entry.fallbackGeometry) {
            try {
                fallbackPath =
                    std::filesystem::weakly_canonical(manifestDir / *entry.fallbackGeometry);
            } catch (const std::exception& e) {
                diagnostics.push_back({entry.id,
                    std::string("fallback path resolution error: ") + e.what(),
                    LoadStatus::LoadedWithWarnings});
            }
        }

        // GPU dedup fast-path: shared immutable Model already uploaded (Full only).
        // Checked BEFORE the CPU parse so the second of two entries sharing a
        // geometry path reports Success with no parse and no diagnostics.
        if (gpuOwns && gpuOwns(canonKey)) {
            rec.origin   = Origin::AlreadyUploaded;
            rec.status   = LoadStatus::Success;
            rec.cacheKey  = canonPath;
            out.push_back(std::move(rec));
            continue;
        }

        // CPU load with dedup by canonical path — shared, immutable, parsed once
        // (this library owns the parse; the registry owns the cache).
        auto        cpu     = cache.loadRecord(canonPath, entry.id, entry.upAxis);
        std::string loadKey = canonKey;
        if (cpu->status == LoadStatus::Failed && fallbackPath &&
            fallbackPath->string() != canonKey) {
            const auto fb = cache.loadRecord(*fallbackPath, entry.id, entry.upAxis);
            if (fb->status != LoadStatus::Failed) {
                loadKey = fallbackPath->string();
                cpu     = fb;
                diagnostics.push_back({entry.id,
                    "geometry failed; explicit fallback_geometry used",
                    LoadStatus::LoadedWithWarnings});
            }
            // fallback also failed → primary failure stands (diagnosed below)
        }

        const auto v = validateCpuRecord(*cpu, entry.id, entry.upAxis, diagnostics);
        if (v.failed) {
            out.push_back(std::move(rec));  // status stays Failed
            continue;
        }

        rec.origin   = Origin::Cpu;
        rec.status   = v.unsafeTextureRef ? LoadStatus::LoadedWithWarnings : cpu->status;
        rec.cpu      = std::move(cpu);
        rec.cacheKey = loadKey;
        out.push_back(std::move(rec));
    }

    return out;
}

}  // namespace noto3d::asset
