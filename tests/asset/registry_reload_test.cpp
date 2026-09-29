// AssetRegistry::reload() — the facade-linking half of the registry contract.
//
// Why these two cases are here and not in registry_test.cpp (ADR-0012 ③):
// reload() spans *both* load modes — it repeats whichever load() or
// loadManifestOnly() ran last — so its implementation must live in the GL
// facade, next to the upload step that Full mode needs.  A CPU-only link can
// therefore not provide the symbol.  The ticket's own scope rule is to split
// such a case into a GL-linked target rather than re-link the whole CPU suite
// to GL, which is exactly what this file is.
//
// The cases themselves are CPU-semantic (both drive ManifestOnly mode) — they
// are here because of where the symbol is defined, not because they need a GPU.
// No case was dropped: the assertions below are the ones removed from
// registry_test.cpp, unchanged.
//
// Note these cases deliberately do NOT call load() and then reload(): that would
// need a live GL context, and the Full-mode path is covered end-to-end by the
// CLI/E2E matrix instead.

#include "asset_registry.hpp"
#include "log.hpp"

#include <cstdlib>
#include <filesystem>

// NOTO3D_ASSETS_DIR is defined by CMake (absolute path to assets/)
#ifndef NOTO3D_ASSETS_DIR
#error "NOTO3D_ASSETS_DIR must be set by the build system"
#endif

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

using namespace noto3d::asset;

void testReload() {
    const std::filesystem::path manifest(NOTO3D_ASSETS_DIR "/manifest.json");
    AssetRegistry reg;
    const auto first = reg.loadManifestOnly(manifest);
    check(first.overall != LoadStatus::Failed, "reload: initial load not Failed");

    const std::size_t firstCount = reg.entries().size();
    check(firstCount > 0, "reload: entries present after first load");

    const auto r2 = reg.reload();
    check(r2.overall != LoadStatus::Failed,      "reload: succeeds");
    check(reg.entries().size() == firstCount,    "reload: same entry count");
    check(reg.findEntry("smile") != nullptr,     "reload: smile still present");
}

void testReloadBeforeLoad() {
    AssetRegistry reg;
    const auto r = reg.reload();
    check(r.overall == LoadStatus::Failed,
          "reload before load: returns Failed");
    check(!r.diagnostics.empty(),
          "reload before load: diagnostic present");
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-asset-reload-tests");

    testReload();
    testReloadBeforeLoad();

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
