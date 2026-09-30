#pragma once

// ADR-0008 declaration-only-header rule: constants with external linkage are
// declared here and defined in scene_io.cpp.  The serialization module does
// not include asset, renderer, or UI headers — the architecture guard in
// CMakeLists.txt enforces this at configure time (ticket 0041).

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// scene.hpp is included rather than forward-declared: std::optional<Scene> and
// std::vector<Entity> in the signatures below need a COMPLETE type at
// instantiation, so a forward declaration cannot compile.  This costs the header
// a glm dependency, which is already how every other scene-facing header in the
// project reads, and the include closure stays inside src/scene (ADR-0013 Q11).
#include "scene.hpp"

namespace noto3d::serialization {

// Version of the JSON document format produced and consumed by this module.
// Consumers must check this field before reading any other key.
// Increment this when the on-disk format changes in a backward-incompatible way.
extern const int kSceneFormatVersion;

// Load result for deserializeScene. The shape is frozen in ticket 0043 so 0043
// and 0044 cannot each invent half a contract.
struct LoadResult {
    std::optional<scene::Scene> scene;         // engaged iff the load succeeded
    std::vector<std::string>     diagnostics;  // empty on a clean success
};

// Serialize a Scene to v1 JSON text (pure function). Always emits "version": 1.
// Entities serialized in Scene::entities() order.
std::string serializeScene(const scene::Scene& scene);

// Deserialize JSON text to a Scene (happy path + version gate). Never throws.
// Structural failures yield scene == nullopt and at least one diagnostic (0044
// completes other paths; version gate behavior is present here).
LoadResult deserializeScene(std::string_view text);

}  // namespace noto3d::serialization
