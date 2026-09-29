#pragma once

#include "asset_cpu.hpp"
#include "manifest_types.hpp"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace noto3d::renderer {
// GPU-owning types (ADR-0012 ①).  Declared but not defined here so this header —
// and every CPU-only consumer of AssetRegistry — resolves no GL header.  The
// registry only holds them behind shared_ptr and hands out borrowed pointers, so
// an incomplete type is enough; the definitions live in the noto3d_asset facade.
//
// Model is an alias, not a class, so it must be re-declared exactly as model.hpp
// declares it: `using Model = std::vector<SubMesh>` over an incomplete SubMesh.
// std::vector supports an incomplete element type (C++17) for declaration, which
// is all the shared_ptr/pointer members below require.
struct SubMesh;
using Model = std::vector<SubMesh>;

class Texture;
}  // namespace noto3d::renderer

namespace noto3d::asset {

// Read-only runtime view of assets/manifest.json (ADR-0007).
//
// Lifecycle:
//   1. load(path)          — parse manifest + upload geometry to GPU (requires GL context).
//   2. loadManifestOnly()  — parse manifest + OBJ to CPU only (no GL); model() returns null.
//      Use for headless tests and pre-GL inspection.
//   3. reload()            — repeat the last load() or loadManifestOnly() call.
//
// Contracts (ADR-0007):
//   * model() returns null if geometry is unavailable or failed — never a cube fallback (ADR-0001).
//   * No exceptions thrown during load; per-asset FAILED is recorded as a diagnostic.
//   * Deduplication: two entries sharing the same canonical geometry path share one
//     CPU parse (uniqueGeometryCount) and, in Full mode, one Model.
//   * Explicit reload() only; no filesystem watcher.
//
// Link boundary (ADR-0012 ③).  This one class spans both libraries, and which
// half you link decides whether you pull in GL:
//   * noto3d_asset_cpu — loadManifestOnly(), loadModelShapesOnly() and every
//     read-only accessor.  No GL symbol is referenced, so a headless test or
//     CLI links no GL at all.
//   * noto3d_asset (facade) — load(), loadModelFile() and reload().  reload()
//     lives here because it spans both modes, so a caller that can reach Full
//     mode can repeat a Full load.
class AssetRegistry {
public:
    AssetRegistry() = default;

    // Parse manifest + OBJ + GL upload.  No-throw; geometry errors → FAILED diagnostic.
    [[nodiscard]] RegistryResult load(const std::filesystem::path& manifestPath);

    // Parse manifest + OBJ to CPU only (no GL upload).  model() returns null for all entries.
    [[nodiscard]] RegistryResult loadManifestOnly(const std::filesystem::path& manifestPath);

    // Repeat the last load() / loadManifestOnly() call.
    [[nodiscard]] RegistryResult reload();

    // ── Standalone (manifest-independent) OBJ/MTL loads ────────────────────
    // For the editor's demo/reference assets that live outside manifest.json
    // (e.g. the 0014 demo_checker fixture).  No AssetEntry is created; the
    // model is exposed via externalModel().  CPU/GPU dedup, ADR-0007 path
    // safety and the 0013 texture predicate all still apply.

    // Assemble + dedup one OBJ/MTL to CPU only (no GL).  Mirrors
    // loadManifestOnly()'s safety/warning surface for a single file.
    [[nodiscard]] RegistryResult loadModelShapesOnly(const std::filesystem::path& objPath,
                                                     std::string_view             label,
                                                     const std::string&           upAxis = "y");

    // Full (GL) standalone OBJ/MTL → GPU Model.  Returns null on failure
    // (diagnostics recorded).  Uses the same texture upload path as manifest
    // loads; reference_image never applies.
    [[nodiscard]] const renderer::Model* loadModelFile(const std::filesystem::path& objPath,
                                                       std::string_view             label,
                                                       const std::string&           upAxis = "y");

    // GPU model produced by the last loadModelFile(); null until one succeeds.
    const renderer::Model* externalModel() const noexcept;

    // Manifest entry for the given id; null if not found.
    const AssetEntry* findEntry(std::string_view id) const noexcept;

    // Span of all entries from the last load.
    std::span<const AssetEntry> entries() const noexcept;

    // GPU model for the given id; null if geometry-less, failed, or loadManifestOnly was used.
    // Never returns a cube fallback (ADR-0001).
    const renderer::Model* model(std::string_view id) const noexcept;

    // Per-asset load status.
    LoadStatus assetStatus(std::string_view id) const noexcept;

    // All diagnostics from the last load / reload.
    std::span<const LoadDiagnostic> diagnostics() const noexcept;

    // Number of distinct canonical geometry paths currently loaded (CPU dedup width).
    // Headless-observable part of the dedup contract (ADR-0007).
    [[nodiscard]] std::size_t uniqueGeometryCount() const noexcept;

private:
    enum class Mode { Full, ManifestOnly };

    struct Record {
        std::shared_ptr<renderer::Model> model;
        LoadStatus                       status = LoadStatus::Failed;
    };

    std::filesystem::path       m_manifestPath;
    Mode                        m_lastMode  = Mode::Full;
    std::vector<AssetEntry>     m_entries;
    std::vector<Record>         m_records;   // parallel to m_entries
    std::vector<LoadDiagnostic> m_diagnostics;

    // Canonical absolute path → shared immutable Model (GPU dedup, Full mode only).
    std::unordered_map<std::string, std::shared_ptr<renderer::Model>> m_modelCache;

    // Texture path → shared GL texture (Full mode only; reference_image never enters here).
    std::unordered_map<std::string, std::shared_ptr<renderer::Texture>> m_textureCache;

    // Standalone (non-manifest) load state — cleared by every load like the caches.
    std::shared_ptr<const CpuObj>    m_externalCpu;
    std::filesystem::path            m_externalObjDir;      // texture base dir
    std::string                      m_externalCanonKey;    // GPU dedup key
    std::shared_ptr<renderer::Model> m_externalModel;

    // CPU parse dedup by canonical path (ADR-0007), owned by this registry.  The
    // parsing itself lives in noto3d_asset_cpu (ADR-0012 ②): one authority, split
    // across two libraries — the registry still owns both caches.
    CpuCache                         m_cpuCache;

    // Clear every cache and the standalone state, then remember this load's
    // request so reload() can repeat it (ADR-0007: explicit reload only).
    void resetForLoad(const std::filesystem::path& manifestPath, Mode mode);

    // Parse the manifest, seed the diagnostics and run the CPU pipeline, leaving
    // m_entries populated.  Shared by both drivers so the two modes cannot differ
    // in how a manifest is read; the caller then attaches models to the returned
    // outcomes.  `gpuOwns` is empty for ManifestOnly.
    std::vector<CpuEntryLoad> runManifestPipeline(const std::filesystem::path& manifestPath,
                                                 const GpuDedupProbe&         gpuOwns = {});

    // The two load drivers, split by the GL boundary (ADR-0012 ③) so that
    // loadManifestOnly() and the read-only accessors link from noto3d_asset_cpu
    // with no GPU symbol in sight.  Each is implemented in the library that owns
    // its half: doLoadManifestOnly() in the CPU core, doLoadFull() in the facade
    // (it performs the upload through uploadToGpu(), the single GL gate).
    // Both drive the *same* noto3d_asset_cpu pipeline, so the CPU half cannot
    // drift between the modes.
    RegistryResult doLoadManifestOnly(const std::filesystem::path& manifestPath);
    RegistryResult doLoadFull(const std::filesystem::path& manifestPath);

    RegistryResult buildResult() const;
};

}  // namespace noto3d::asset
