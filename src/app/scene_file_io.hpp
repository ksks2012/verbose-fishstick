#pragma once

// ADR-0013 Q6: file I/O lives in the APP layer.  The serializer
// (noto3d_serialization) stays a pure text<->Scene function with no filesystem
// knowledge and no way to reach the registry, which is what makes 0044's
// structure/reference split structural rather than conventional.  Editor stays an
// I/O-free semantic facade, so nothing here needs new Editor API (Q6): the caller
// passes a Scene in and gets a Scene out.
//
// ADR-0008 declaration-only-header rule: every definition lives in the .cpp.

#include <scene.hpp>
#include <scene_io.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace noto3d::app {

// Read a whole file into `out`. Returns false (after logging a CRITICAL
// diagnostic naming the path) if it cannot be opened or read.
//
// A missing or unreadable file is a FAILURE, never an empty string: "" would
// deserialize as a malformed document, turning "file not found" into a
// confusing structural diagnostic far from the real cause.
bool readSceneFile(const std::filesystem::path& path, std::string& out);

// Write `text` to `path` ATOMICALLY (ADR-0013 Q8): a uniquely-named temp file in
// the DESTINATION DIRECTORY, then rename() into place.  An existing destination
// is overwritten SILENTLY — no prompt, no --force gate.
//
// Two properties this function exists to guarantee:
//  - A missing PARENT DIRECTORY is a failure.  It is never mkdir'd: silently
//    growing a directory tree hides a typo in the path until much later.
//  - Any write error removes the temp file before returning, so a failed save
//    leaves NO residue beside the previous good document — which is also what
//    keeps that document intact, since rename() is the only step that touches
//    it.
//
// Returns false after logging a CRITICAL diagnostic naming the path.
bool writeSceneFileAtomic(const std::filesystem::path& path, std::string_view text);

// serializeScene() + writeSceneFileAtomic(). A convenience so the CLI has one
// call per direction rather than pairing the two by hand at every site.
bool saveSceneTo(const std::filesystem::path& path, const scene::Scene& scene);

// readSceneFile() + deserializeScene(). Returns a scene-less LoadResult (with
// diagnostics) if EITHER step fails, so a caller gets one failure shape to
// handle and the serializer's "never throws" contract is preserved.
serialization::LoadResult loadSceneFrom(const std::filesystem::path& path);

// The file stem, used as the scene identity (ADR-0013 Q9) so the frozen
// M4_INFO / window-title shapes are reused unchanged:
//   "path/to/myscene.scene" -> "myscene"
// Dots in parent directories do not leak in, and a name with no extension is
// returned whole.
std::string sceneNameFromPath(const std::filesystem::path& path);

}  // namespace noto3d::app
