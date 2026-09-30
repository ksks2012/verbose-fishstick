#include "scene_file_io.hpp"

#include "log.hpp"

#include <atomic>
#include <cstdio>
#include <fstream>
#include <system_error>
#include <unistd.h>  // getpid, for the per-process temp-name uniqueness

namespace noto3d::app {
namespace {

// A per-process counter for the temp name. rename() is atomic only for a SINGLE
// writer, so uniqueness is what makes the guarantee hold in practice: two
// concurrent saves (or a re-entrant one) sharing a fixed "out.scene.tmp" would
// clobber each other's partial file and then rename the WRONG bytes into place.
std::atomic<unsigned> g_tempCounter{0};

std::filesystem::path makeTempPath(const std::filesystem::path& destination) {
    // The temp file lives in the DESTINATION DIRECTORY, not /tmp: rename() is
    // only atomic within one filesystem, so a temp elsewhere would turn the
    // rename into a non-atomic copy.
    const std::filesystem::path dir =
        destination.has_parent_path() ? destination.parent_path()
                                      : std::filesystem::path(".");
    const std::string stem = destination.stem().string();
    return dir / ("." + stem + ".tmp." + std::to_string(::getpid()) + "." +
                  std::to_string(g_tempCounter.fetch_add(1) + 1));
}

}  // namespace

bool readSceneFile(const std::filesystem::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        NOTO3D_CRITICAL("M6_FAIL scene file '{}' could not be opened for reading", path.string());
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        NOTO3D_CRITICAL("M6_FAIL scene file '{}' could not be read", path.string());
        return false;
    }
    out = std::move(text);
    return true;
}

bool writeSceneFileAtomic(const std::filesystem::path& path, std::string_view text) {
    const std::filesystem::path parent =
        path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");

    std::error_code ec;
    // Q8: a missing parent directory is a diagnostic, NEVER a mkdir.  Growing a
    // directory tree here would silently paper over a typo in the path.
    if (!std::filesystem::is_directory(parent, ec)) {
        // ec is reported when it carries something: an unreadable directory
        // (EACCES/EPERM) also makes is_directory() false, and calling that
        // "does not exist" would send the user hunting for a typo that is not
        // there.
        if (ec) {
            NOTO3D_CRITICAL("M6_FAIL scene save: parent directory '{}' is unusable: {}",
                            parent.string(), ec.message());
        } else {
            NOTO3D_CRITICAL("M6_FAIL scene save: parent directory '{}' does not exist",
                            parent.string());
        }
        return false;
    }

    const std::filesystem::path temp = makeTempPath(path);
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            NOTO3D_CRITICAL("M6_FAIL scene save: could not create temp file '{}'",
                            temp.string());
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        // The close() error is checked explicitly: a short write on a full disk
        // surfaces here, and ignoring it is how a truncated file gets renamed
        // into place as if it were complete.
        out.close();
        if (out.fail()) {
            out.clear();
            std::filesystem::remove(temp, ec);
            NOTO3D_CRITICAL("M6_FAIL scene save: writing '{}' failed", temp.string());
            return false;
        }
    }

    // rename() replaces the destination atomically and SILENTLY (Q8): no prompt,
    // no --force gate.
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        // Clean up on ANY failure, so a failed save leaves no residue beside the
        // previous good document (which rename() never touched).
        std::error_code cleanup;
        std::filesystem::remove(temp, cleanup);
        NOTO3D_CRITICAL("M6_FAIL scene save: could not write '{}': {}", path.string(),
                        ec.message());
        return false;
    }
    return true;
}

bool saveSceneTo(const std::filesystem::path& path, const scene::Scene& scene) {
    return writeSceneFileAtomic(path, serialization::serializeScene(scene));
}

serialization::LoadResult loadSceneFrom(const std::filesystem::path& path) {
    serialization::LoadResult result;
    std::string             text;
    if (!readSceneFile(path, text)) return result;

    result = serialization::deserializeScene(text);
    // The serializer's diagnostics are the user's actionable content (0044's
    // taxonomy); they are re-emitted verbatim rather than replaced, so the path
    // context sits alongside them instead of discarding them.
    for (const std::string& d : result.diagnostics) {
        NOTO3D_CRITICAL("M6_FAIL scene '{}': {}", path.string(), d);
    }
    return result;
}

std::string sceneNameFromPath(const std::filesystem::path& path) {
    // stem() strips the FINAL extension only, so "dir.with.dots/out.scene"
    // yields "out" and a dot in a parent directory never leaks in.
    return path.stem().string();
}

}  // namespace noto3d::app
