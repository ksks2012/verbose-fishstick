#pragma once

#include "manifest_types.hpp"
#include <model.hpp>

#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

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
//   * Deduplication: two entries sharing the same canonical geometry path share one Model.
//   * Explicit reload() only; no filesystem watcher.
class AssetRegistry {
public:
    AssetRegistry() = default;

    // Parse manifest + OBJ + GL upload.  No-throw; geometry errors → FAILED diagnostic.
    [[nodiscard]] RegistryResult load(const std::filesystem::path& manifestPath);

    // Parse manifest + OBJ to CPU only (no GL upload).  model() returns null for all entries.
    [[nodiscard]] RegistryResult loadManifestOnly(const std::filesystem::path& manifestPath);

    // Repeat the last load() / loadManifestOnly() call.
    [[nodiscard]] RegistryResult reload();

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

    // Canonical absolute path → shared immutable Model (dedup cache).
    std::unordered_map<std::string, std::shared_ptr<renderer::Model>> m_modelCache;

    RegistryResult doLoad(const std::filesystem::path& manifestPath, Mode mode);
    RegistryResult buildResult() const;
};

}  // namespace noto3d::asset
