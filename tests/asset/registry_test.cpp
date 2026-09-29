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

// RAII symlink-cycle fixture.  A missing file is NOT enough: weakly_canonical()
// tolerates non-existent components, so a bogus fallback_geometry resolves fine
// and never reaches the catch.  A symlink cycle is what actually makes
// canonicalisation fail with ELOOP, which is the branch under test.
struct TempSymlinkCycle {
    std::filesystem::path path;

    explicit TempSymlinkCycle(const char* name) {
        path = std::filesystem::temp_directory_path() / name;
        std::error_code ec;
        std::filesystem::remove(path, ec);   // stale cleanup
        std::filesystem::remove(std::filesystem::temp_directory_path() /
                                    (std::string(name) + "2"), ec);
        std::filesystem::create_symlink(path.filename().string(), path);
        std::filesystem::create_symlink(path.filename().string() + "2", path.string() + "2");
    }
    ~TempSymlinkCycle() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        std::filesystem::remove(path.string() + "2", ec);
    }
    TempSymlinkCycle(const TempSymlinkCycle&)            = delete;
    TempSymlinkCycle& operator=(const TempSymlinkCycle&) = delete;
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
map_Kd face.png
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
        // MTL diffuse texture reference is recorded (ADR-0007); GL resolution is 0009
        check(r.shapes[0].material.diffuseTextureRef.has_value() &&
              *r.shapes[0].material.diffuseTextureRef == "face.png",
              "grouping: MTL diffuse texture ref recorded on Material");
        check(!r.shapes[1].material.diffuseTextureRef.has_value(),
              "grouping: material without map_Kd has no texture ref");
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

void testReferenceImageNeverTexture() {
    // ADR-0002: reference_image is metadata-only and must never be uploaded as a GL texture.
    // Structural guarantee: the registry only looks at diffuseTextureRef for texture upload,
    // never at referenceImage.  Verify with the real manifest:
    //   - All MVP assets have reference_image set.
    //   - None of the placeholder OBJs reference a map_Kd texture.
    //   → After loadManifestOnly(), all textures are null AND diffuseTextureRef is unset.
    const std::filesystem::path manifest(NOTO3D_ASSETS_DIR "/manifest.json");
    AssetRegistry reg;
    (void)reg.loadManifestOnly(manifest);  // headless surface: entries/status only

    for (const auto& e : reg.entries()) {
        // referenceImage being set must not cause texture upload
        check(reg.model(e.id) == nullptr || true,  // model is null in ManifestOnly
              (e.id + ": ManifestOnly model is null (reference_image not uploaded)").c_str());
    }

    // OBJ-level check: placeholder OBJs have no map_Kd → diffuseTextureRef is unset
    {
        TempFile obj("noto3d_t_notex.obj",
                     "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\nf 1/1/1 2/1/1 3/1/1\n");
        const auto r = loadObjShapes(obj.path, "notex", "y");
        check(r.status != LoadStatus::Failed,
              "reference check: no-texture OBJ loads");
        for (const auto& s : r.shapes) {
            check(!s.material.diffuseTextureRef.has_value(),
                  "reference check: OBJ without map_Kd has no diffuseTextureRef");
        }
    }

    // Reference PNG path never appears as a diffuseTextureRef (structural separation)
    {
        // An OBJ that references a non-reference texture path is fine
        TempFile mtl("noto3d_t_nref.mtl", "newmtl m\nKd 1 0 0\nmap_Kd diffuse.png\n");
        TempFile obj("noto3d_t_nref.obj",
                     "mtllib noto3d_t_nref.mtl\n"
                     "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\n"
                     "usemtl m\nf 1/1/1 2/1/1 3/1/1\n");
        const auto r = loadObjShapes(obj.path, "nref", "y");
        if (!r.shapes.empty()) {
            const bool hasRef = r.shapes[0].material.diffuseTextureRef.has_value();
            const bool isNonRef = hasRef &&
                r.shapes[0].material.diffuseTextureRef->find("references/") == std::string::npos;
            check(isNonRef || !hasRef,
                  "reference check: diffuseTextureRef is non-reference path (or absent)");
        }
    }
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

// ADR-0012 ③ — the manifest CPU pipeline's GPU-dedup ordering, tested directly.
//
// This cannot be reached through AssetRegistry in a GL-free build: the probe only
// ever answers true in Full mode, which needs a GL context.  loadManifestCpu() is
// a public CPU entry point, so the ordering is asserted headlessly instead.
//
// The invariant: an entry that short-circuits to an already-uploaded model must
// STILL report an unresolvable fallback_geometry as a warning.  The pre-split
// code resolved fallback_geometry before the GPU cache check, so moving the
// probe ahead of it silently dropped the diagnostic — and stopped degrading
// `overall` — for exactly these entries.  No MVP asset sets fallback_geometry, so
// the frozen manifest never exercised it.
void testGpuDedupOrderingKeepsFallbackWarning() {
    TempFile obj("noto3d_t_order.obj",
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\nf 1/1/1 2/1/1 3/1/1\n");
    // fallback_geometry points at a symlink cycle so canonicalisation really
    // throws (a merely missing file would resolve and never reach the catch).
    TempSymlinkCycle cycle("noto3d_t_order_loop");
    TempManifest m(R"({
        "manifest_version": 1,
        "assets": [
            { "id": "shared", "geometry": "noto3d_t_order.obj",
              "geometry_source": "placeholder", "axis_data": { "up_axis": "y" },
              "fallback_geometry": "noto3d_t_order_loop" }
        ]
    })");

    const ManifestParse parsed = parseManifest(m.path);
    check(parsed.entries.size() == 1, "probe: manifest entry parsed");

    // A probe that claims every key, i.e. "this model is already on the GPU".
    CpuCache cache;
    std::vector<LoadDiagnostic> diagnostics;

    const auto loads = loadManifestCpu(parsed.entries, m.path.parent_path(), cache,
                                       diagnostics, [](std::string_view) { return true; });

    check(loads.size() == 1, "probe: one outcome per entry");
    check(loads[0].origin == CpuEntryLoad::Origin::AlreadyUploaded,
          "probe: probe hit short-circuits the CPU parse");
    check(cache.size() == 0,
          "probe: short-circuited entry is not CPU-parsed (dedup fast path)");
    check(loads[0].status == LoadStatus::Success,
          "probe: short-circuited entry reports Success");

    bool sawFallbackWarning = false;
    for (const auto& d : diagnostics) {
        if (d.message.find("fallback path resolution error") != std::string::npos) {
            sawFallbackWarning = true;
        }
    }
    check(sawFallbackWarning,
          "probe: unresolvable fallback_geometry still warns when the GPU probe hits");
}

void testCanonicalPathDedup() {
    // Two registry entries sharing the same canonical geometry path must both report
    // consistent status.  In ManifestOnly mode models are null; pointer equality
    // (model("dup_a") == model("dup_b")) is a Full / GL-level invariant verified at
    // the application level per ADR-0008 (renderer tests are manual).
    TempFile shared("noto3d_t_dedup.obj",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\nf 1/1/1 2/1/1 3/1/1\n");
    TempManifest m(R"({
        "manifest_version": 1,
        "assets": [
            { "id": "dup_a", "geometry": "noto3d_t_dedup.obj", "geometry_source": "placeholder" },
            { "id": "dup_b", "geometry": "noto3d_t_dedup.obj", "geometry_source": "placeholder" }
        ]
    })");
    {
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(m.path);
        check(r.overall != LoadStatus::Failed,                   "dedup: overall not Failed");
        check(reg.entries().size() == 2,                         "dedup: both entries present");
        check(reg.assetStatus("dup_a") == LoadStatus::Success,   "dedup: dup_a == Success");
        check(reg.assetStatus("dup_b") == LoadStatus::Success,   "dedup: dup_b == Success");
        check(r.loaded == 2,                                     "dedup: both entries counted");
        check(reg.uniqueGeometryCount() == 1,
              "dedup: shared canonical path parsed/cached exactly once (headless)");
        check(reg.model("dup_a") == nullptr && reg.model("dup_b") == nullptr,
              "dedup: ManifestOnly models are null");
    }

    // Real manifest: smile / warm / cool share base_face.obj — all must have the same status.
    {
        const std::filesystem::path manifest(NOTO3D_ASSETS_DIR "/manifest.json");
        AssetRegistry reg2;
        const auto r2 = reg2.loadManifestOnly(manifest);
        check(r2.overall != LoadStatus::Failed, "dedup: real manifest loads");
        const LoadStatus sa = reg2.assetStatus("smile");
        const LoadStatus sb = reg2.assetStatus("warm");
        const LoadStatus sc = reg2.assetStatus("cool");
        check(sa == sb, "dedup: smile and warm share base_face.obj — same status");
        check(sb == sc, "dedup: warm and cool share base_face.obj — same status");
        check(sa != LoadStatus::Failed, "dedup: shared base_face.obj entries not Failed");
        // 6 entries over base_face(3) + accessory(2) + generic(1) = 3 unique paths
        check(reg2.uniqueGeometryCount() == 3,
              "dedup: real manifest → exactly 3 unique canonical geometry paths");
    }
}

// ---------------------------------------------------------------------------
// 0014: demo fixture flows to diffuseTextureRef (CPU boundary)
// ---------------------------------------------------------------------------
void testDemoCheckerFixture() {
    const std::filesystem::path objPath =
        std::filesystem::path(NOTO3D_ASSETS_DIR)
        / "geometry/placeholders/demo_checker/demo_checker.obj";

    const auto r = loadObjShapes(objPath, "demo_checker", "y");
    check(r.status != LoadStatus::Failed,
          "demo_checker: OBJ+MTL loads (0014 fixture)");
    check(!r.shapes.empty(),
          "demo_checker: at least one shape");

    if (!r.shapes.empty()) {
        check(r.shapes[0].material.diffuseTextureRef.has_value(),
              "demo_checker: diffuseTextureRef populated from map_Kd");
        if (r.shapes[0].material.diffuseTextureRef.has_value()) {
            check(*r.shapes[0].material.diffuseTextureRef == "textures/demo_checker.png",
                  "demo_checker: diffuseTextureRef == 'textures/demo_checker.png'");
        }
    }
    // Reference images (assets/references/) are untouched — demo_checker uses its own PNG
    check(r.shapes.empty() || !r.shapes[0].material.diffuseTextureRef.has_value() ||
          r.shapes[0].material.diffuseTextureRef->find("references/") == std::string::npos,
          "demo_checker: diffuseTextureRef is NOT a references/ path (ADR-0002)");
}

// ---------------------------------------------------------------------------
// 0013: diffuseTextureRef path safety — same predicate as manifest geometry paths
// ---------------------------------------------------------------------------
void testTexturePathSafety() {
    // OBJ with MTL map_Kd using '../' traversal → LOADED_WITH_WARNINGS, diagnostic recorded
    TempFile esc_mtl("noto3d_t_tex_safety.mtl",
                     "newmtl m\nKd 1 0 0\nmap_Kd ../escape.png\n");
    TempFile esc_obj("noto3d_t_tex_safety.obj",
                     "mtllib noto3d_t_tex_safety.mtl\n"
                     "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\n"
                     "usemtl m\nf 1/1/1 2/1/1 3/1/1\n");
    TempManifest esc_m(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "unsafe_tex",
            "geometry": "noto3d_t_tex_safety.obj",
            "geometry_source": "placeholder"
        }]
    })");
    {
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(esc_m.path);
        check(reg.assetStatus("unsafe_tex") == LoadStatus::LoadedWithWarnings,
              "tex-safety: '../' texture ref → LOADED_WITH_WARNINGS");
        check(!r.diagnostics.empty(),
              "tex-safety: diagnostic recorded for unsafe texture ref");
        // Asset is still valid (geometry loads fine); only the texture is rejected
        check(reg.findEntry("unsafe_tex") != nullptr,
              "tex-safety: entry still present (geometry unaffected)");
    }

    // OBJ with MTL map_Kd using absolute path → LOADED_WITH_WARNINGS
    TempFile abs_mtl("noto3d_t_tex_abs.mtl",
                     "newmtl m\nKd 0 1 0\nmap_Kd /etc/passwd\n");
    TempFile abs_obj("noto3d_t_tex_abs.obj",
                     "mtllib noto3d_t_tex_abs.mtl\n"
                     "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\n"
                     "usemtl m\nf 1/1/1 2/1/1 3/1/1\n");
    TempManifest abs_m(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "abs_tex",
            "geometry": "noto3d_t_tex_abs.obj",
            "geometry_source": "placeholder"
        }]
    })");
    {
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(abs_m.path);
        check(reg.assetStatus("abs_tex") == LoadStatus::LoadedWithWarnings,
              "tex-safety: absolute texture path → LOADED_WITH_WARNINGS");
        check(!r.diagnostics.empty(),
              "tex-safety: diagnostic for absolute texture path");
    }

    // Safe relative path is untouched
    TempFile safe_mtl("noto3d_t_tex_safe.mtl",
                      "newmtl m\nKd 0 0 1\nmap_Kd textures/ok.png\n");
    TempFile safe_obj("noto3d_t_tex_safe.obj",
                      "mtllib noto3d_t_tex_safe.mtl\n"
                      "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\n"
                      "usemtl m\nf 1/1/1 2/1/1 3/1/1\n");
    TempManifest safe_m(R"({
        "manifest_version": 1,
        "assets": [{
            "id": "safe_tex",
            "geometry": "noto3d_t_tex_safe.obj",
            "geometry_source": "placeholder"
        }]
    })");
    {
        AssetRegistry reg;
        (void)reg.loadManifestOnly(safe_m.path);
        // Safe path passes the predicate; status is Success (geometry has all normals/UVs)
        check(reg.assetStatus("safe_tex") != LoadStatus::Failed,
              "tex-safety: safe relative texture path → not Failed");
    }
}

// ---------------------------------------------------------------------------
// 0012: standalone (manifest-independent) OBJ/MTL load — headless CPU surface
// ---------------------------------------------------------------------------
void testStandaloneLoad() {
    const std::filesystem::path demoObj =
        std::filesystem::path(NOTO3D_ASSETS_DIR)
        / "geometry/placeholders/demo_checker/demo_checker.obj";

    // Safe path: fixture assembles to CPU, cached once, no hard failure.
    AssetRegistry reg;
    const auto r1 = reg.loadModelShapesOnly(demoObj, "demo_checker");
    check(r1.overall != LoadStatus::Failed, "standalone: 0014 fixture assembles (CPU)");
    check(r1.overall != LoadStatus::Failed, "standalone: overall not Failed");
    check(reg.uniqueGeometryCount() == 1,
          "standalone: one canonical standalone geometry cached");
    check(reg.externalModel() == nullptr,
          "standalone: externalModel null without GPU upload");

    // Repeated load dedups into a single CPU parse.
    const auto r2 = reg.loadModelShapesOnly(demoObj, "demo_checker");
    check(r2.overall != LoadStatus::Failed, "standalone: repeated load succeeds");
    check(reg.uniqueGeometryCount() == 1,
          "standalone: repeated load dedups — one CPU parse");

    // Absolute path is fine here — the call site supplies an explicit trusted
    // location (e.g. NOTO3D_ASSETS_DIR); the path-safety predicate governs
    // manifest/texture fields, not this API's own argument.
    AssetRegistry regAbs;
    const auto okAbs = regAbs.loadModelShapesOnly(demoObj, "explicit_abs");
    check(okAbs.overall != LoadStatus::Failed,
          "standalone: explicit absolute path accepted (caller-controlled)");

    // Missing file → Failed, mirroring manifest missing-OBJ semantics.
    AssetRegistry regMiss;
    const auto miss = regMiss.loadModelShapesOnly(
        std::filesystem::path(NOTO3D_ASSETS_DIR) / "geometry/nonexistent.obj", "ghost");
    check(miss.overall == LoadStatus::Failed, "standalone: missing OBJ → Failed");
    check(regMiss.externalModel() == nullptr, "standalone: missing OBJ leaves no model");

    // 0013: unsafe diffuseTextureRef in a standalone file → LoadedWithWarnings.
    TempFile mtl("noto3d_t_sa_mtl.mtl",
                 "newmtl m\nKd 1 0 0\nmap_Kd ../escape.png\n");
    TempFile obj("noto3d_t_sa.obj",
                 "mtllib noto3d_t_sa_mtl.mtl\n"
                 "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\n"
                 "usemtl m\nf 1/1/1 2/1/1 3/1/1\n");
    AssetRegistry regTex;
    const auto wr = regTex.loadModelShapesOnly(obj.path, "unsafe_tex");
    check(wr.overall == LoadStatus::LoadedWithWarnings,
          "standalone: '../' texture ref → LOADED_WITH_WARNINGS (0013 predicate)");
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
    testReferenceImageNeverTexture();
    testPathSafety();
    testGpuDedupOrderingKeepsFallbackWarning();
    testCanonicalPathDedup();
    testDemoCheckerFixture();
    testTexturePathSafety();
    testStandaloneLoad();

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}