#include "draw_list.hpp"
#include "log.hpp"

#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

bool nearMat(const glm::mat4& a, const glm::mat4& b) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (std::abs(a[c][r] - b[c][r]) > 1e-5f) return false;
    return true;
}

// Empty GPU resources — never construct Mesh/upload; just valid pointer addresses
// for resolver injection.  No GL context required.
const noto3d::renderer::Model kFakeModelA;
const noto3d::renderer::Model kFakeModelB;

using namespace noto3d::app;
using namespace noto3d::scene;

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-app-tests");

    // ── Test 1: resolved entity produces exactly one DrawItem ────────────────
    {
        Scene sc;
        sc.createEntity("cube", "smile");
        Resolver resolve = [](const std::string& id) -> const noto3d::renderer::Model* {
            return (id == "smile") ? &kFakeModelA : nullptr;
        };
        const auto items = collectDrawItems(sc, resolve);
        check(items.size() == 1, "resolved: 1 DrawItem emitted");
        if (!items.empty()) {
            check(items[0].model == &kFakeModelA, "resolved: model pointer correct");
            check(items[0].assetId == "smile",    "resolved: assetId correct");
        }
    }

    // ── Test 2: unresolved entity skipped; other entities still emitted ──────
    {
        Scene sc;
        sc.createEntity("cube",  "smile");    // resolves
        sc.createEntity("ghost", "unknown");  // does NOT resolve → skip
        sc.createEntity("cube2", "warm");     // resolves
        Resolver resolve = [](const std::string& id) -> const noto3d::renderer::Model* {
            if (id == "smile") return &kFakeModelA;
            if (id == "warm")  return &kFakeModelB;
            return nullptr;
        };
        const auto items = collectDrawItems(sc, resolve);
        check(items.size() == 2, "unresolved: 2 of 3 entities emitted");
        if (items.size() == 2) {
            check(items[0].assetId == "smile", "unresolved: first item is smile");
            check(items[1].assetId == "warm",  "unresolved: third entity → second item");
        }
    }

    // ── Test 3: N entities with the same assetId → N DrawItems (no dedup) ───
    {
        Scene sc;
        sc.createEntity("e1", "smile");
        sc.createEntity("e2", "smile");
        sc.createEntity("e3", "smile");
        Resolver resolve = [](const std::string& id) -> const noto3d::renderer::Model* {
            return (id == "smile") ? &kFakeModelA : nullptr;
        };
        const auto items = collectDrawItems(sc, resolve);
        check(items.size() == 3, "no dedup: 3 entities same assetId → 3 DrawItems");
        if (items.size() == 3) {
            check(items[0].model == &kFakeModelA &&
                  items[1].model == &kFakeModelA &&
                  items[2].model == &kFakeModelA,
                  "no dedup: all 3 hold the same model pointer");
        }
    }

    // ── Test 4: modelMatrix == scene::modelMatrix(entity.transform) ──────────
    {
        Scene sc;
        const auto eid = sc.createEntity("cube", "smile");
        Entity* e = sc.findEntity(eid);
        e->transform.position = {1.5f, -2.0f,  0.5f};
        e->transform.rotation = {0.1f,  0.4f, -0.2f};
        e->transform.scale    = {2.0f,  1.0f,  0.5f};

        const glm::mat4 expected = modelMatrix(e->transform);

        Resolver resolve = [](const std::string&) -> const noto3d::renderer::Model* {
            return &kFakeModelA;
        };
        const auto items = collectDrawItems(sc, resolve);
        check(items.size() == 1, "modelMatrix: 1 item");
        if (!items.empty()) {
            check(nearMat(items[0].modelMatrix, expected),
                  "modelMatrix == scene::modelMatrix(entity.transform)");
        }
    }

    // ── Test 5: empty Resolver → std::logic_error (never silent empty list) ─
    {
        Scene sc;
        sc.createEntity("cube", "smile");
        Resolver empty;  // default-constructed = empty / null
        bool threw = false;
        try {
            collectDrawItems(sc, empty);
        } catch (const std::logic_error&) {
            threw = true;
        } catch (...) {}
        check(threw, "empty resolver → std::logic_error");
    }

    // ── Test 6: empty scene → empty draw list ────────────────────────────────
    {
        Scene sc;
        Resolver resolve = [](const std::string&) -> const noto3d::renderer::Model* {
            return &kFakeModelA;
        };
        check(collectDrawItems(sc, resolve).empty(),
              "empty scene → empty draw list");
    }

    // ── Test 7: creation-order preserved ─────────────────────────────────────
    {
        Scene sc;
        sc.createEntity("a", "cool");
        sc.createEntity("b", "smile");
        sc.createEntity("c", "warm");
        Resolver resolve = [](const std::string& id) -> const noto3d::renderer::Model* {
            if (id == "cool")  return &kFakeModelA;
            if (id == "smile") return &kFakeModelB;
            if (id == "warm")  return &kFakeModelA;
            return nullptr;
        };
        const auto items = collectDrawItems(sc, resolve);
        check(items.size() == 3, "ordering: 3 items");
        if (items.size() == 3) {
            check(items[0].assetId == "cool"  &&
                  items[1].assetId == "smile" &&
                  items[2].assetId == "warm",
                  "ordering: creation order preserved");
        }
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
