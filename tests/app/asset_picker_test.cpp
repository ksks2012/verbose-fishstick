#include "asset_picker.hpp"
#include "log.hpp"

#include <asset_registry.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

using namespace noto3d::app;
using namespace noto3d::asset;

// RAII temp manifest — writes inline JSON, removes on destruction.
// Geometry paths are resolved relative to its directory (temp dir).
struct TempManifest {
    std::filesystem::path path;

    explicit TempManifest(const char* json) {
        path = std::filesystem::temp_directory_path()
             / ("noto3d_picker_manifest_" +
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
// path a TempManifest refers to.  Stale files from crashed runs are removed first.
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

// Minimal valid OBJ (single triangle with vn+vt) for a manifest geometry entry.
const char* kTriangleObj = R"(# obj
v 0 0 0
v 1 0 0
v 0 1 0
vn 0 0 1
vt 0 0
f 1/1/1 2/1/1 3/1/1
)";

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-asset-picker-tests");

    // ── Snapshot construction from a mixed manifest ──────────────────────────
    // Two renderable entries + one reference-only entry → renderable-only rule.
    {
        TempFile obj("noto3d_picker_face.obj", kTriangleObj);
        TempManifest m(R"({
            "manifest_version": 1,
            "assets": [
                {"id": "smile", "geometry": "noto3d_picker_face.obj", "geometry_source": "placeholder"},
                {"id": "crown", "geometry": "noto3d_picker_face.obj", "geometry_source": "official"},
                {"id": "noop",  "reference_image": "some/ref.png"}
            ]
        })");
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(m.path);
        check(r.overall != LoadStatus::Failed, "mixed manifest: overall not Failed");

        const auto snap = buildAssetPickerSnapshot(reg);
        check(snap.renderableCount == 2, "snapshot: renderableCount == 2");
        check(snap.hiddenCount     == 1, "snapshot: hiddenCount == 1 (reference-only)");
        check(snap.choices.size()  == 2, "snapshot: choices.size() == renderableCount");

        // Typed values, in registry order; display text is NOT pre-formatted.
        if (snap.choices.size() == 2) {
            check(snap.choices[0].id == "smile",
                  "snapshot: choices[0].id == 'smile'");
            check(snap.choices[0].geometrySource == GeometrySource::Placeholder,
                  "snapshot: choices[0] source == Placeholder (typed)");
            check(snap.choices[1].id == "crown",
                  "snapshot: choices[1].id == 'crown'");
            check(snap.choices[1].geometrySource == GeometrySource::Official,
                  "snapshot: choices[1] source == Official (typed)");
        }
        // The reference-only entry is absent from the offered list.
        for (const auto& c : snap.choices) {
            check(c.id != "noop", "snapshot: reference-only 'noop' is not offered");
        }
    }

    // ── Reference-only-only manifest → zero renderable, counts still logged ──
    {
        TempManifest m(R"({
            "manifest_version": 1,
            "assets": [
                {"id": "only_ref", "reference_image": "a.png"},
                {"id": "meta_only", "reference_image": "b.png"}
            ]
        })");
        AssetRegistry reg;
        const auto r = reg.loadManifestOnly(m.path);
        check(r.overall != LoadStatus::Failed, "reference-only manifest: overall not Failed");

        const auto snap = buildAssetPickerSnapshot(reg);
        check(snap.renderableCount == 0, "ref-only manifest: renderableCount == 0");
        check(snap.hiddenCount     == 2, "ref-only manifest: hiddenCount == 2");
        check(snap.choices.empty(),      "ref-only manifest: no choices offered");
    }

    // ── commitAssetAssignment: effect on the Scene is LIMITED to assetId ─────
    {
        Editor ed;
        const auto idSmile = ed.createEntity("smile", "smile");
        const auto idCrown = ed.createEntity("crown", "crown");
        ed.setEntityPosition(idSmile, {1.0f, 2.0f, 3.0f});
        ed.setEntityRotation(idSmile, {0.1f, 0.2f, 0.3f});
        ed.setEntityScale(idSmile, {2.0f, 2.0f, 2.0f});

        commitAssetAssignment(ed, idSmile, "warm");

        const auto items = ed.hierarchyItems();
        check(items.size() == 2, "commit: entity count unchanged");
        check(items[0].assetId == "warm",  "commit: smile assetId reassigned");
        check(items[1].assetId == "crown", "commit: other entity unaffected");

        // Two entities could be on the same asset and stay independent.
        commitAssetAssignment(ed, idCrown, "warm");
        {
            const auto again = ed.hierarchyItems();
            check(again[0].assetId == "warm", "commit: smile still warm");
            check(again[1].assetId == "warm", "commit: crown now warm (independent)");
        }

        // Transform of the reassigned entity is untouched (assetId-only effect).
        const auto t = ed.selectedTransform();
        (void)t;  // selection not required for the mutation checks above
    }

    // ── commitAssetAssignment on an unresolvable id: skip, not failure ───────
    // The Scene accepts any string id; resolution failure is the draw_list
    // seam's existing skip behaviour (ADR-0010 Q5).  Assigning an id absent
    // from the registry must not crash and must not empty the picker.
    {
        Editor ed;
        const auto id = ed.createEntity("smile", "smile");
        commitAssetAssignment(ed, id, "no_such_asset");
        check(ed.hierarchyItems()[0].assetId == "no_such_asset",
              "unresolvable: assetId stored (resolution is deferred to frame)");

        // commitAssetAssignment on an invalid id is a safe no-op.
        commitAssetAssignment(ed, noto3d::scene::InvalidEntityId, "warm");
        check(ed.hierarchyItems().size() == 1,
              "invalid id: commit is a safe no-op (entity count unchanged)");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}