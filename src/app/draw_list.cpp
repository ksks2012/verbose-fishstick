#include "draw_list.hpp"

#include <cassert>
#include <stdexcept>

namespace noto3d::app {

std::vector<DrawItem> collectDrawItems(const scene::Scene& scene,
                                       const Resolver&     resolve) {
    // Empty Resolver is a programming error; throw std::logic_error immediately
    // so the caller can never get a silent empty draw list (ADR-0010 Q5).
    if (!resolve) {
        throw std::logic_error(
            "collectDrawItems: Resolver is empty — always provide a callable "
            "resolver; an empty std::function is a programming error, not a "
            "supported runtime state");
    }
    assert(resolve && "collectDrawItems: Resolver must not be empty");

    std::vector<DrawItem> items;
    for (const auto& entity : scene.entities()) {
        const renderer::Model* model = resolve(entity.assetId);
        if (!model) {
            continue;  // unresolved asset: skip entity, not failure (ADR-0010 Q5)
        }
        items.push_back({model,
                         entity.assetId,
                         scene::modelMatrix(entity.transform)});
    }
    return items;
}

}  // namespace noto3d::app
