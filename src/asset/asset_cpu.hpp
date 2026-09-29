#pragma once

#include "manifest_types.hpp"
#include "obj_loader.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace noto3d::asset {

// ---------------------------------------------------------------------------
// Asset CPU core (ADR-0012 ②) — the GL-free half of the asset library.
//
// Everything reachable from this header parses, validates and de-duplicates
// assets on the CPU.  It links no `noto3d_renderer` and resolves no GL header:
// the only `noto3d::renderer` types it names are the two GL-free interchange
// structs `Vertex` and `Material` (ADR-0012 ①).  GPU upload is NOT here — it
// lives in the `noto3d_asset` facade, and this library stays the CPU half of
// one authority, never a second authority (ADR-0007).
// ---------------------------------------------------------------------------

// Shared CPU parse record for one canonical geometry path (ADR-0007 dedup).
struct CpuObj {
    LoadStatus                  status;
    std::vector<CanonicalShape> shapes;
    int                         normalWarnings = 0;
    int                         uvWarnings     = 0;
    std::string                 errorMessage;
};

// ADR-0007 path-safety predicate (ticket 0013).  Returns true when a path is
// unsafe for asset loading:
//   * contains a '../' traversal sequence
//   * is absolute (starts with '/' or '\')
// Applies to manifest geometry/fallback_geometry fields AND to every material
// `diffuseTextureRef` (ticket 0013), on both the CPU and the GPU side — it is
// declared here so the split keeps exactly one definition of the predicate.
[[nodiscard]] bool isUnsafePath(std::string_view p) noexcept;

// One manifest.json parsed to CPU: the accepted entries plus the
// manifest-level diagnostics produced while validating them.
struct ManifestParse {
    std::vector<AssetEntry>     entries;
    std::vector<LoadDiagnostic> diagnostics;
};

// Read and validate manifest.json (pure CPU, no GL).  Enforces the ADR-0007
// invariants: stable `id`; geometry/geometry_source non-null iff each other;
// relative paths with no '../' (via isUnsafePath).
[[nodiscard]] ManifestParse parseManifest(const std::filesystem::path& manifestPath);

// Push the parse-recorded diagnostics (errorMessage / normalWarnings /
// uvWarnings) for a CPU result.  Used identically by manifest loads and
// standalone loads.
void appendCpuDiagnostics(std::vector<LoadDiagnostic>& out,
                          const CpuObj&                 cpu,
                          std::string_view               label);

// Ticket 0013: reject diffuseTextureRef paths that violate the ADR-0007
// predicate (absolute or '../').  Each rejection is recorded as a warning
// diagnostic.  Returns true when at least one ref was rejected.
[[nodiscard]] bool scanUnsafeTextureRefs(std::vector<LoadDiagnostic>& out,
                                        const CpuObj&                 cpu,
                                        std::string_view               label);

// Aggregate per-asset statuses and their diagnostics into a RegistryResult.
// Pure CPU: the tri-state counts come from the record statuses, and `overall`
// also accounts for every diagnostic severity.  Lives here so the GL-free CLI
// path (ADR-0012 ③, backing `--list-assets`) can produce a RegistryResult
// without linking the GPU facade.
[[nodiscard]] RegistryResult aggregateResults(std::span<const LoadStatus>     statuses,
                                             std::span<const LoadDiagnostic> diagnostics);

// CPU-side deduplication by canonical geometry path (ADR-0007): a path is
// parsed at most once and the resulting immutable CpuObj is shared.  Failed
// parses are deliberately NOT cached, so a later load of the same path
// re-diagnoses rather than replaying a stale failure.
class CpuCache {
public:
    // Parse (or return the shared record for) one canonical OBJ path.
    [[nodiscard]] std::shared_ptr<const CpuObj> loadRecord(
        const std::filesystem::path& path,
        std::string_view               label,
        const std::string&             upAxis);

    // Drop every cached parse (explicit reload only; ADR-0007 has no watcher).
    void clear() noexcept;

    // Number of distinct canonical geometry paths currently cached — the
    // headless-observable half of the dedup contract.
    [[nodiscard]] std::size_t size() const noexcept;

private:
    // Canonical absolute path → shared immutable CPU parse record.
    std::unordered_map<std::string, std::shared_ptr<const CpuObj>> m_records;
};

}  // namespace noto3d::asset
