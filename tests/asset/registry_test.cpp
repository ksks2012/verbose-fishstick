#include "asset_registry.hpp"
#include "log.hpp"
#include "obj_loader.hpp"

#include <glm/glm.hpp>

#include <cmath>
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

bool near(float a, float b) { return std::abs(a - b) < 1e-5f; }

using namespace noto3d::asset;

// RAII temp manifest — writes inline JSON, removes on destruction.
// Geometry paths are resolved relative to its directory (temp dir).
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

// RAII temp file with a fixed literal name in temp dir — matches the relative
// path a TempManifest refers to. Stale files from crashed runs are removed first.
struct TempFile {
    std::filesystem::path path;

    explicit TempFile(const char* name, const std::string& content) {
        path = std::filesystem::temp_directory_path() / name;
        std::error_code ec;
        std::filesystem::remove(path, ec);  // stale cleanup
        std::ofstream f(path);
        f << content;
    }
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    TempFile(const TempFile&)            = delete;
    TempFile& operator=(const TempFile&) = delete;
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
    const auto loadR = reg.loadManifestOnly(manifest);
    check(loadR.overall != LoadStatus::Failed, "geometry load: manifest not Failed");

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

void testObjLoaderMaterialGrouping() {
    // One OBJ, two usemtl regions → two canonical shapes with distinct materials
    TempFile obj("noto3d_t_group.obj", R"(mtllib noto3d_t_group.mtl
v 0 0 0
v 1 0 0
v 0 1 0
v 0 0 1
vn 0 0 1
vt 0 0
usemtl red
f 1/1/1 2/1/1 3/1/1
usemtl blue
f 1/1/1 2/1/1 4/1/1
)");
    TempFile mtl("noto3d_t_group.mtl", R"(newmtl red
Kd 1 0 0
newmtl blue
Kd 0 0 1
)");

    const auto r = loadObjShapes(obj.path, "group", "y");
    check(r.status != LoadStatus::Failed, "grouping: OBJ + MTL loads");
    check(r.shapes.size() == 2,            "grouping: two usemtl regions → two shapes");
    if (r.shapes.size() == 2) {
        check(near(r.shapes[0].material.baseColor.r, 1.0f) &&
              near(r.shapes[0].material.baseColor.g, 0.0f) &&
              near(r.shapes[0].material.baseColor.b, 0.0f),
              "grouping: first material == red (Kd 1 0 0)");
        check(near(r.shapes[1].material.baseColor.b, 1.0f) &&
              near(r.shapes[1].material.baseColor.r, 0.0f),
              "grouping: second material == blue (Kd 0 0 1)");
        check(r.shapes[0].indices.size() == 3 && r.shapes[1].indices.size() == 3,
              "grouping: 3 indices per sub-shape");
    }
}

void testAxisBaking() {
    // up_axis "z": vertex (0,0,1) must be baked to (0,1,0) (Rx(-90°))
    TempFile obj("noto3d_t_axis.obj", R"(v 0 0 1
v 0 0 0
v 1 0 0
vn 0 0 1
vt 0 0
f 1/1/1 2/1/1 3/1/1
)");

    const auto r = loadObjShapes(obj.path, "axis", "z");
    check(r.status != LoadStatus::Failed, "axis: loads");

    bool found = false;
    for (const auto& s : r.shapes) {
        for (const auto& v : s.vertices) {
            if (near(v.position.x, 0.0f) && near(v.position.y, 1.0f) &&
                near(v.position.z, 0.0f)) {
                found = true;
            }
        }
    }
    check(found, "axis: source +Z vertex baked to +Y");

    TempFile yobj("noto3d_t_axis_y.obj", R"(v 0 0 1
v 0 0 0
v 1 0 0
vn 0 0 1
vt 0 0
f 1/1/1 2/1/1 3/1/1
)");
    const auto ry = loadObjShapes(yobj.path, "axisy", "y");
    bool unchanged = false;
    for (const auto& s : ry.shapes) {
        for (const auto& v : s.vertices) {
            if (near(v.position.y, 0.0f) && near(v.position.z, 1.0f)) {
                unchanged = true;
            }
        }
    }
    check(unchanged, "axis: up_axis 'y' leaves geometry untouched");
}

void testTriStateOverall() {
    // SUCCESS: manifest + valid geometry (real placeholder asset tree)
    {
        const std::filesystem::path manifest(NOTO3D_ASSETS_DIR "/manifest.json");
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(manifest);
        check(r.overall != LoadStatus::Failed,
              "tri-state: real manifest not Failed");
    }

    // Mixed: one resolvable OBJ + one missing OBJ → overall Failed, loaded==1, failed==1
    {
        TempFile good("noto3d_t_good.obj",
                      "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\nf 1/1/1 2/1/1 3/1/1\n");
        TempManifest m(R"({
            "manifest_version": 1,
            "assets": [
                { "id": "ok",     "geometry": "noto3d_t_good.obj", "geometry_source": "placeholder" },
                { "id": "broken", "geometry": "noto3d_t_missing.obj", "geometry_source": "placeholder" }
            ]
        })");
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(m.path);
        check(r.overall == LoadStatus::Failed,
              "tri-state: one broken → overall Failed");
        check(r.loaded == 1 && r.failed == 1,
              "tri-state: loaded==1 and failed==1");
        check(reg.assetStatus("ok") == LoadStatus::Success,
              "tri-state: ok asset == Success");
        check(reg.assetStatus("broken") == LoadStatus::Failed,
              "tri-state: broken asset == Failed");
        check(reg.model("ok") == nullptr,
              "tri-state: ManifestOnly model is null");
    }

    // Mixed: full-coverage OBJ + normals-computed OBJ → overall LoadedWithWarnings
    {
        TempFile full("noto3d_t_full.obj",
                      "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\nf 1/1/1 2/1/1 3/1/1\n");
        TempFile nonorm("noto3d_t_nonorm.obj",
                        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        TempManifest m(R"({
            "manifest_version": 1,
            "assets": [
                { "id": "full",   "geometry": "noto3d_t_full.obj",   "geometry_source": "placeholder" },
                { "id": "nonorm", "geometry": "noto3d_t_nonorm.obj", "geometry_source": "placeholder" }
            ]
        })");
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(m.path);
        check(r.overall == LoadStatus::LoadedWithWarnings,
              "tri-state: missing-normals asset → overall LoadedWithWarnings");
        check(r.loaded == 1 && r.warned == 1,
              "tri-state: loaded==1 and warned==1");
        check(reg.assetStatus("full") == LoadStatus::Success,
              "tri-state: full asset == Success");
        check(reg.assetStatus("nonorm") == LoadStatus::LoadedWithWarnings,
              "tri-state: nonorm asset == LoadedWithWarnings");
    }
}

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
    testObjLoaderMaterialGrouping();
    testAxisBaking();
    testTriStateOverall();
    testReload();
    testReloadBeforeLoad();
    testPathSafety();

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}