#pragma once

#include <model.hpp>
#include <scene.hpp>

#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace noto3d::app {

// Frame-local description of one entity resolved for rendering.
//
// Lifetime contract (ADR-0010 Q5):
//   model is a BORROWED pointer — valid only within the frame in which
//   collectDrawItems() was called.  A DrawItem must never be retained across
//   AssetRegistry::reload() or any registry mutation; the same invalidation
//   discipline that governs Scene references (spans from scene.entities()
//   are invalidated by structural mutation) applies to these model pointers.
//   Each frame must re-obtain a fresh draw list.
struct DrawItem {
    const renderer::Model* model;        // borrowed, no ownership
    std::string            assetId;      // diagnostic identity
    glm::mat4              modelMatrix;  // == scene::modelMatrix(entity.transform)
};

// Callable that maps a stable asset ID to the associated GPU Model.
// Returns nullptr when the asset is unavailable or geometry-less.
//
// Programming-error contract: an empty (default-constructed) Resolver is never
// a valid runtime state.  collectDrawItems() throws std::logic_error on an
// empty Resolver — it will never silently return an empty draw list because
// the Resolver was forgotten.
using Resolver = std::function<const renderer::Model*(const std::string&)>;

// Build a frame-local draw list from a Scene.
//
//   * Iterates scene.entities() in creation order.
//   * resolve(entity.assetId) == nullptr  ⇒  entity skipped (no DrawItem emitted;
//     this is NOT a failure; the Scene is never modified).
//   * Two entities with the same assetId ⇒ two DrawItems (no deduplication;
//     asset identity ≠ entity identity, ADR-0010 Q5).
//   * No class, no cache, no state stored back into Scene or Entity.
//
// Throws std::logic_error if resolve is empty (programming error; never
// std::bad_function_call, never a silent empty list).
std::vector<DrawItem> collectDrawItems(const scene::Scene& scene,
                                       const Resolver&     resolve);

}  // namespace noto3d::app
