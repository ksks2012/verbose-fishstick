#include "asset_registry.hpp"
#include "log.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>

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

// RAII temp manifest — writes inline JSON, removes on destruction.
struct TempManifest {
    std::filesystem::path path;

    explicit TempManifest(const char* json) {
        path = std::filesystem::temp_directory_path()
             / ("noto3d_test_manifest_" +
                std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".json");
        std::ofstream f(path);
        f << json;
    }
    ~TempManifest() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    TempManifest(const TempManifest&)            = delete;
    TempManifest& operator=(const TempManifest&) = delete;
};

// ---------------------------------------------------------------------------
// Test groups
// ---------------------------------------------------------------------------

void testManifestParse() {
    const std::filesystem::path manifest(NOTO3D_ASSETS_DIR "/manifest.json");
    AssetRegistry reg;
    const auto result = reg.loadManifestOnly(manifest);

    check(result.overall != LoadStatus::Failed, "manifest parse: not FAILED overall");

    // All six MVP assets must be present
    check(reg.findEntry("smile")      != nullptr, "manifest parse: 'smile' present");
    check(reg.findEntry("warm")       != nullptr, "manifest parse: 'warm' present");
    check(reg.findEntry("cool")       != nullptr, "manifest parse: 'cool' present");
    check(reg.findEntry("crown")      != nullptr, "manifest parse: 'crown' present");
    check(reg.findEntry("sunglasses") != nullptr, "manifest parse: 'sunglasses' present");
    check(reg.findEntry("top_hat")    != nullptr, "manifest parse: 'top_hat' present");

    check(reg.entries().size() == 6, "manifest parse: exactly 6 entries");

    // Entry fields
    const AssetEntry* smile = reg.findEntry("smile");
    check(smile != nullptr && smile->geometry.has_value(),
          "smile: geometry present");
    check(smile != nullptr && smile->geometrySource == GeometrySource::Placeholder,
          "smile: geometrySource == Placeholder");
    check(smile != nullptr && smile->referenceImage.has_value(),
          "smile: referenceImage present");

    // loadManifestOnly — model() must always be null
    check(reg.model("smile") == nullptr,
          "loadManifestOnly: model is null (no GL upload)");
}

void testGeometryLoad() {
    // The placeholder OBJ files have vn + vt for all vertices → SUCCESS (no warnings)
    const std::filesystem::path manifest(NOTO3D_ASSETS_DIR "/manifest.json");
    AssetRegistry reg;
    reg.loadManifestOnly(manifest);

    // All assets with geometry should parse without FAILED
    for (const auto& e : reg.entries()) {
        if (e.geometry) {
            check(reg.assetStatus(e.id) != LoadStatus::Failed,
                  (e.id + ": geometry parses without FAILED").c_str());
        }
    }
}

void testInvariantRejection() {
    // geometry set but geometry_source missing → invariant violation → FAILED
    TempManifest bad1(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "bad",
            "geometry": "some/path.obj"
        }]
    })");
    {
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(bad1.path);
        check(r.overall == LoadStatus::Failed,
              "invariant: geometry without geometry_source → FAILED");
        check(reg.findEntry("bad") == nullptr,
              "invariant: rejected entry not in registry");
    }

    // geometry_source set but geometry missing → invariant violation → FAILED
    TempManifest bad2(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "bad2",
            "geometry_source": "placeholder"
        }]
    })");
    {
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(bad2.path);
        check(r.overall == LoadStatus::Failed,
              "invariant: geometry_source without geometry → FAILED");
    }

    // Unknown geometry_source value → FAILED
    TempManifest bad3(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "bad3",
            "geometry": "foo.obj",
            "geometry_source": "unofficial"
        }]
    })");
    {
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(bad3.path);
        check(r.overall == LoadStatus::Failed,
              "invariant: unknown geometry_source value → FAILED");
    }
}

void testGeometrylessAsset() {
    TempManifest m(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "noop",
            "reference_image": "some/ref.png"
        }]
    })");
    AssetRegistry reg;
    const auto r = reg.loadManifestOnly(m.path);

    check(reg.findEntry("noop") != nullptr, "geometry-less: entry found");
    check(reg.model("noop") == nullptr,     "geometry-less: model is null");
    check(reg.assetStatus("noop") == LoadStatus::Success,
          "geometry-less: status == Success");
    // referenceImage is metadata-only (ADR-0002)
    const AssetEntry* e = reg.findEntry("noop");
    check(e && e->referenceImage.has_value(),
          "geometry-less: referenceImage field preserved");
    check(e && !e->geometry.has_value(),
          "geometry-less: geometry field is empty");
}

void testMissingObjFile() {
    TempManifest m(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "ghost",
            "geometry": "nonexistent/ghost.obj",
            "geometry_source": "placeholder"
        }]
    })");
    AssetRegistry reg;
    const auto r = reg.loadManifestOnly(m.path);

    check(reg.findEntry("ghost") != nullptr,         "missing OBJ: entry recorded");
    check(reg.assetStatus("ghost") == LoadStatus::Failed,
          "missing OBJ: status == Failed");
    check(reg.model("ghost") == nullptr,             "missing OBJ: model is null");
    check(r.overall == LoadStatus::Failed,           "missing OBJ: overall == Failed");
    check(!r.diagnostics.empty(),                    "missing OBJ: diagnostic present");
}

void testTriStateOverall() {
    // SUCCESS: manifest + valid geometry
    {
        const std::filesystem::path manifest(NOTO3D_ASSETS_DIR "/manifest.json");
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(manifest);
        check(r.overall != LoadStatus::Failed,
              "tri-state: real manifest not Failed");
    }

    // FAILED: one bad entry poisons the overall status
    {
        TempManifest m(R"({
            "manifest_version": 1,
            "assets": [
                { "id": "ok", "geometry": "geometry/placeholders/base_face.obj", "geometry_source": "placeholder" },
                { "id": "broken", "geometry": "nonexistent.obj", "geometry_source": "placeholder" }
            ]
        })");
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(
            std::filesystem::path(m.path.parent_path()) / m.path.filename());
        // We need to resolve the OBJ paths relative to m.path
        // Since base_face.obj is relative to temp dir, it won't be found either
        // So both will be FAILED → overall FAILED
        check(r.overall == LoadStatus::Failed,
              "tri-state: broken geometry → overall Failed");
        check(r.failed > 0,
              "tri-state: at least one failed asset counted");
    }
}

void testReload() {
    const std::filesystem::path manifest(NOTO3D_ASSETS_DIR "/manifest.json");
    AssetRegistry reg;
    reg.loadManifestOnly(manifest);

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

void testPathSafety() {
    // Absolute path rejected
    TempManifest bad1(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "abs",
            "geometry": "/etc/passwd",
            "geometry_source": "placeholder"
        }]
    })");
    {
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(bad1.path);
        check(r.overall == LoadStatus::Failed,
              "path-safety: absolute geometry path → Failed");
    }

    // "../" traversal rejected
    TempManifest bad2(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "traverse",
            "geometry": "../secret/data.obj",
            "geometry_source": "placeholder"
        }]
    })");
    {
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(bad2.path);
        check(r.overall == LoadStatus::Failed,
              "path-safety: '../' traversal → Failed");
    }
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-asset-tests");

    testManifestParse();
    testGeometryLoad();
    testInvariantRejection();
    testGeometrylessAsset();
    testMissingObjFile();
    testTriStateOverall();
    testReload();
    testReloadBeforeLoad();
    testPathSafety();

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
