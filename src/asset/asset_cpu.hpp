#pragma once

#include "manifest_types.hpp"
#include "obj_loader.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
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

// The outcome of the shared per-entry CPU epilogue.
struct CpuValidation {
    // The parse failed: its diagnostics have been recorded and nothing further
    // may be applied to this entry (no axis warning, no texture scan).  Stops the
    // pipeline exactly where a failed parse stopped it before.
    bool failed = false;
    // The declared up_axis was not x/y/z, so no axis correction was applied.
    bool unsupportedAxis = false;
    // At least one diffuseTextureRef was rejected by the ADR-0007 predicate.
    bool unsafeTextureRef = false;
};

// The shared tail of every CPU load: parse diagnostics, then the defensive
// up-axis warning and the 0013 texture-ref predicate.  Both entry points — the
// manifest pipeline and the standalone load — run this, so the ADR-0007 rules
// have one implementation and cannot drift between them.
//
// Status *accounting* deliberately stays with the caller: the manifest pipeline
// folds the outcome into a per-asset record, the standalone path into a single
// RegistryResult.  Neither is specified by ADR-0007, so neither is shared.
[[nodiscard]] CpuValidation validateCpuRecord(const CpuObj&                   cpu,
                                              std::string_view                 label,
                                              const std::string&               upAxis,
                                              std::vector<LoadDiagnostic>& out);

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

// ---------------------------------------------------------------------------
// The manifest CPU pipeline — one implementation, three callers
// ---------------------------------------------------------------------------
//
// This is the whole ADR-0007 per-entry CPU contract in one place: canonical-path
// resolution, GPU-dedup ordering, the explicit `fallback_geometry` opt-in, the
// 0013 `diffuseTextureRef` predicate and the up-axis warning.  It exists so the
// three consumers below run *the same code* instead of three lookalike copies:
//   1. AssetRegistry::loadManifestOnly()  — the headless path the tests use;
//   2. AssetRegistry::load()              — its CPU half, before GPU upload;
//   3. runListAssetsCommand()             — `--list-assets` (ADR-0009 0017),
//      which must print byte-identical output without the registry or the
//      facade (ADR-0012 ③).
// Keeping (3) on this function is what makes "identical" a structural property
// rather than a coincidence that happens to hold for today's clean manifest.

// Full mode's GPU dedup fast path (ADR-0007), consulted with the canonical
// geometry key *before* any CPU parse.  When two manifest entries share one
// geometry path, the second records Success with no parse and no diagnostics —
// that ordering is observable, so it is preserved here rather than
// reconstructed by each caller.  Return true to skip the CPU work for the key.
using GpuDedupProbe = std::function<bool(std::string_view canonicalKey)>;

// One manifest entry's CPU outcome.  Returned parallel to the input entries, so
// index i always describes entries[i] — including for geometry-less entries,
// which are valid and never a failure (ADR-0007).
struct CpuEntryLoad {
    // What decided this outcome, so a GPU caller can dispatch exhaustively
    // rather than inferring it from a null pointer.
    enum class Origin {
        GeometryLess,     // entry.geometry == nullopt — no geometry, status Success
        AlreadyUploaded,  // the probe claimed the key; nothing was parsed
        Cpu,              // a CPU parse ran; `cpu` holds its result
    };

    Origin                        origin = Origin::Cpu;
    LoadStatus                    status = LoadStatus::Failed;
    std::shared_ptr<const CpuObj> cpu;  // null unless origin == Cpu and status != Failed

    // The manifest entry's id, so a GPU caller can label upload diagnostics
    // without re-indexing the input entries.
    std::string                   id;

    // Directory that `diffuseTextureRef` is resolved against: the *primary*
    // geometry's directory, even when `fallback_geometry` supplied the parse.
    // Unchanged from the pre-split behaviour, where a fallback load still
    // resolved textures beside the primary geometry.
    std::filesystem::path         textureBaseDir;

    // The canonical path actually parsed (primary, or the fallback that
    // succeeded) — the GPU dedup key.  Empty for GeometryLess.
    std::filesystem::path         cacheKey;
};

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

// Run the manifest CPU pipeline for already-parsed entries.  Appends every
// diagnostic it produces to `diagnostics` (which the caller typically pre-seeds
// with the manifest-level ones) and returns one outcome per entry, in order.
// `cache` provides the canonical-path dedup; the caller owns and clears it, so
// `CpuCache::size()` stays the headless-observable dedup width (ADR-0007).
// `gpuOwns` is empty for CPU-only callers and holds the fast-path probe in
// Full mode.  No-throw: geometry errors are reported as FAILED diagnostics.
[[nodiscard]] std::vector<CpuEntryLoad> loadManifestCpu(
    std::span<const AssetEntry>  entries,
    const std::filesystem::path& manifestDir,
    CpuCache&                    cache,
    std::vector<LoadDiagnostic>& diagnostics,
    const GpuDedupProbe&         gpuOwns = {});

}  // namespace noto3d::asset
