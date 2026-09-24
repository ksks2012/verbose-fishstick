#pragma once

#include <optional>
#include <string>
#include <vector>

namespace noto3d::asset {

// Geometry source classification per manifest entry (ADR-0007).
enum class GeometrySource { Placeholder, Official };

// Tri-state load result for one asset or the whole registry (ADR-0007).
enum class LoadStatus { Success, LoadedWithWarnings, Failed };

// One entry exactly as declared in manifest.json (ADR-0007).
// Invariant: geometrySource is non-null if and only if geometry is non-null.
struct AssetEntry {
    std::string                   id;
    std::optional<std::string>    referenceImage;    // metadata-only; never a runtime texture (ADR-0002)
    std::optional<std::string>    geometry;          // relative path; null = geometry-less
    std::optional<GeometrySource> geometrySource;    // invariant: non-null iff geometry non-null
    std::optional<std::string>    fallbackGeometry;  // explicit manifest opt-in only (ADR-0007)
    std::string                   upAxis           = "y";
    std::optional<std::string>    recordedWinding;   // diagnostic only; never drives GL state
};

// One diagnostic message associated with an asset or the manifest.
struct LoadDiagnostic {
    std::string assetId;    // empty = manifest-level error
    std::string message;
    LoadStatus  severity;
};

// Aggregate result returned by load() and reload().
struct RegistryResult {
    LoadStatus                  overall = LoadStatus::Success;
    int                         loaded  = 0;   // assets with SUCCESS
    int                         warned  = 0;   // assets with LOADED_WITH_WARNINGS
    int                         failed  = 0;   // assets with FAILED
    std::vector<LoadDiagnostic> diagnostics;
};

}  // namespace noto3d::asset
