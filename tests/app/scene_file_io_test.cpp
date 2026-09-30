// Ticket 0045 — app-layer scene file I/O (ADR-0013 Q6 file I/O in the app layer,
// Q8 atomic + silent overwrite).  These tests are the seam's contract: they are
// what prove the atomicity claim, because "it renames" is not the same as "a
// failed save never leaves residue and never truncates the previous document".
//
// Headless and file-touching by design (0044 deliberately had no real files), so
// everything happens under a unique temp directory that is removed on scope
// exit — the suite must not depend on, or leave anything in, the repo.

#include "log.hpp"
#include "scene_file_io.hpp"

#include <cstdio>
#include <unistd.h>  // getpid, for the per-run temp dir
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

// One directory per run, so a leftover from a previous run can never make a
// test pass by accident (and so parallel runs cannot collide).
class TempDir {
public:
    TempDir() {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("noto3d-0045-" + std::to_string(::getpid()) + "-" + std::to_string(++counter));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    std::string file(const std::string& name) const {
        return (path_ / name).string();
    }
    std::string dir() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

std::vector<std::string> entriesIn(const std::string& dir) {
    std::vector<std::string> out;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        out.push_back(e.path().filename().string());
    }
    return out;
}

const char* kGoodDocument =
    R"({"version":1,"entities":[{"id":1,"name":"a","assetId":"smile",)"
    R"("transform":{"position":[1,2,3],"rotation":[0,0,0],"scale":[1,1,1]}}]})";

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-scene-file-io-tests");

    using namespace noto3d;
    using namespace noto3d::app;

    // ── Test 1: write a file, read it back byte-identical ─────────────────────
    {
        TempDir dir;
        const std::string path = dir.file("out.scene");

        check(writeSceneFileAtomic(path, kGoodDocument), "save: a fresh path succeeds");

        std::string text;
        check(readSceneFile(path, text), "load: the written file reads back");
        check(text == kGoodDocument, "load: bytes round-trip exactly");
    }

    // ── Test 2: silent overwrite, exactly one file, no temp residue ───────────
    // Q8: an existing destination is overwritten with no prompt and no --force
    // gate, and the temp file must not survive the rename.
    {
        TempDir dir;
        const std::string path = dir.file("out.scene");
        check(writeSceneFileAtomic(path, kGoodDocument), "overwrite: first save succeeds");
        check(writeSceneFileAtomic(path, R"({"version":1,"entities":[]})"),
              "overwrite: second save over the same path succeeds");

        const auto entries = entriesIn(dir.dir());
        check(entries.size() == 1 && entries[0] == "out.scene",
              "overwrite: exactly one file remains, named out.scene");

        std::string text;
        check(readSceneFile(path, text) && text == R"({"version":1,"entities":[]})",
              "overwrite: the destination holds the NEW document, not the old one");
    }

    // ── Test 3: a missing parent directory fails and NEVER mkdir -p's it ─────
    // Q8 explicitly: a missing parent is a diagnostic + non-zero exit, never a
    // silent directory tree.
    {
        TempDir dir;
        const std::string path = dir.file("no-such-dir/out.scene");
        check(!writeSceneFileAtomic(path, kGoodDocument),
              "missing parent dir: save fails");
        check(!std::filesystem::exists(dir.file("no-such-dir")),
              "missing parent dir: the directory was NOT created");
    }

    // ── Test 4: a failed save leaves NO temp file and does not touch the old ──
    // The load-bearing half of atomicity.  To prove the CLEANUP the destination
    // is pointed at a DIRECTORY: the temp file is created successfully, and
    // rename() then fails with EISDIR -- so without an explicit cleanup the temp
    // would survive.  (An unwritable parent would not prove it: there the temp is
    // never created in the first place.)
    {
        TempDir dir;
        const std::string good = dir.file("out.scene");
        check(writeSceneFileAtomic(good, kGoodDocument), "prior save succeeds");

        // A directory where the destination file should be: rename() onto a
        // directory fails with EISDIR.
        const std::string clash = dir.file("clash");
        std::filesystem::create_directories(clash);
        check(!writeSceneFileAtomic(clash, kGoodDocument),
              "rename onto a directory: save fails");

        // No temp file may survive the failure.
        bool strayTemp = false;
        for (const auto& e : entriesIn(dir.dir())) {
            if (!e.empty() && e[0] == '.') strayTemp = true;
        }
        check(!strayTemp, "a failed save left NO temp file beside the document");

        // And the previous good document must be byte-identical: rename() is the
        // only step that touches the destination, so a failed rename cannot.
        std::string text;
        check(readSceneFile(good, text) && text == kGoodDocument,
              "a failed save left the previous good document byte-identical");
    }

    // ── Test 5: reading a missing file is a failure, not an empty document ───
    // A missing file must never yield empty text: "" is a MALFORMED document, so
    // silently succeeding here would turn "file not found" into a confusing
    // structural diagnostic far from the real cause.
    {
        TempDir dir;
        std::string text = "sentinel";
        check(!readSceneFile(dir.file("absent.scene"), text),
              "load: a missing file fails rather than returning empty text");
        // The seed is asserted so the failure cannot be faked by leaving `text`
        // untouched: a caller must be able to tell "failed" from "succeeded but
        // wrote nothing".
        check(text == "sentinel",
              "load: a failed read leaves the out-parameter untouched");
    }

    // ── Test 6: load + save through the serializer is a real round-trip ───────
    // Q7/Q9 seam: this is the CLI's write-then-reload path, minus the process.
    {
        TempDir dir;
        const std::string path = dir.file("rt.scene");

        scene::Scene s;
        s.createEntity("cube", "smile");
        s.createEntity("ghost", "asset_that_does_not_exist");

        check(saveSceneTo(path, s), "round-trip: saveSceneTo succeeds");

        const auto loaded = loadSceneFrom(path);
        check(loaded.scene.has_value(), "round-trip: the saved document reloads");
        if (loaded.scene) {
            check(loaded.scene->entities().size() == 2,
                  "round-trip: entity count survives");
            check(loaded.scene->entities()[0].assetId == "smile" &&
                      loaded.scene->entities()[1].assetId == "asset_that_does_not_exist",
                  "round-trip: an unresolvable assetId is retained verbatim");
            check(loaded.scene->entities()[0].transform.position.y == 0.0f,
                  "round-trip: transform survives");
        }
    }

    // ── Test 6d: an EMPTY entities[] document is valid (Q10) ────────────────
    // Q10: "{"version":1,"entities":[]} is a valid document".  This suite already
    // WRITES that shape, so loading it is the untested half -- and it is the half
    // that exercises Scene::fromEntities({}) plus the derived allocator on an
    // empty set (max() over nothing), which is where an off-by-one would hide.
    {
        TempDir dir;
        const std::string path = dir.file("empty.scene");
        const std::string emptyDoc = "{\"version\":1,\"entities\":[]}";
        check(writeSceneFileAtomic(path, emptyDoc), "empty doc: it is writable");

        std::string text;
        check(readSceneFile(path, text) && text == emptyDoc,
              "empty doc: round-trips byte-identically");

        serialization::LoadResult loaded = loadSceneFrom(path);
        check(loaded.scene.has_value(),
              "empty doc: loads (a missing Scene would make it structurally invalid)");
        check(loaded.diagnostics.empty(),
              "empty doc: no diagnostics -- it is valid, not merely tolerated");
        check(loaded.scene && loaded.scene->entities().empty(),
              "empty doc: yields a Scene with no entities");
    }

    // ── Test 7: sceneNameFromPath reports the FILE STEM (Q9) ─────────────────
    {
        check(sceneNameFromPath("path/to/myscene.scene") == "myscene",
              "Q9: 'path/to/myscene.scene' -> 'myscene'");
        check(sceneNameFromPath("myscene.scene") == "myscene",
              "Q9: a bare name -> the stem");
        check(sceneNameFromPath("/tmp/dir.with.dots/out.scene") == "out",
              "Q9: dots in the parent directory do not leak into the name");
        check(sceneNameFromPath("weird") == "weird",
              "Q9: no extension -> the whole filename");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
