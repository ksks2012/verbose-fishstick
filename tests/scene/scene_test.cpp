#include "scene.hpp"
#include "log.hpp"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) {
        NOTO3D_INFO("PASS  {}", name);
    } else {
        NOTO3D_ERROR("FAIL  {}", name);
        ++g_failures;
    }
}

bool matNear(const glm::mat4& a, const glm::mat4& b, float eps = 1e-5f) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            if (std::abs(a[c][r] - b[c][r]) > eps) return false;
        }
    }
    return true;
}

glm::vec3 transformPoint(const glm::mat4& m, const glm::vec3& p) {
    const auto v = m * glm::vec4(p, 1.0f);
    return glm::vec3(v.x, v.y, v.z);
}

bool vecNear(const glm::vec3& a, const glm::vec3& b, float eps = 1e-5f) {
    return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps &&
           std::abs(a.z - b.z) <= eps;
}

using noto3d::scene::InvalidEntityId;
using noto3d::scene::Scene;
using noto3d::scene::modelMatrix;

void testIdentityAndStorage() {
    Scene s;
    check(s.createEntity("a", "smile") == 1, "first id = 1");
    check(s.createEntity("b", "crown") == 2, "ids are monotonic");
    check(s.createEntity("c", "sun")  == 3, "third id = 3");

    check(s.findEntity(2) != nullptr && s.findEntity(2)->assetId == "crown",
          "findEntity returns the named entity");
    check(s.findEntity(99) == nullptr, "findEntity rejects unknown id");

    // id != storage index: second-created entity is at index 1 with id 2 — trivially
    // equal here, so the invariant is enforced by ORDER independence: erase 2 and
    // index 1 now holds id 3, proving lookups never use the index.
    check(s.destroyEntity(2), "destroy known id returns true");
    check(s.entities().size() == 2, "entity count after erase");
    check(s.entities()[0].id == 1 && s.entities()[1].id == 3,
          "creation order preserved after erase (id != index)");
    check(s.findEntity(2) == nullptr, "destroyed id no longer found");
    check(!s.destroyEntity(2), "destroying unknown id again returns false");

    const auto next = s.createEntity("d", "warm");
    check(next == 4, "destroyed id (2) is never reused");
    check(s.destroyEntity(1) && s.entities().size() == 2 &&
          s.entities()[0].id == 3 && s.entities()[1].id == 4,
          "erase preserves order of remaining entities");
}

void testClearAndCounter() {
    Scene s;
    check(s.createEntity("a", "smile") == 1, "pre-clear id");
    check(s.createEntity("b", "crown") == 2, "pre-clear id 2");
    s.clear();
    check(s.entities().empty(), "clear() removes all entities");
    check(s.createEntity("c", "warm") == 3,
          "clear() does not reset the id counter (next id = 3)");
}

void testEntityDefaults() {
    Scene s;
    const auto id = s.createEntity("hero", "smile");
    const auto* e = s.findEntity(id);
    check(e != nullptr, "entity readable after create");
    check(e->id != InvalidEntityId, "entity id is valid (non-zero)");
    check(e->name == "hero" && e->assetId == "smile", "name/assetId preserved");
    check(e->transform.position == glm::vec3(0.0f) &&
          e->transform.rotation == glm::vec3(0.0f) &&
          e->transform.scale == glm::vec3(1.0f),
          "default transform is identity (pos 0, rot 0, scale 1)");
    check(InvalidEntityId == 0, "InvalidEntityId == 0");
}

void testModelMatrix() {
    using noto3d::scene::Transform;
    using noto3d::scene::modelMatrix;

    check(matNear(modelMatrix(Transform{}), glm::mat4(1.0f)),
          "modelMatrix(identity) == identity");

    Transform t;
    t.position = glm::vec3(1.0f, 2.0f, 3.0f);
    check(vecNear(transformPoint(modelMatrix(t), glm::vec3(0.0f)), t.position),
          "translation moves origin to position");

    Transform sc;
    sc.scale = glm::vec3(2.0f, 3.0f, 4.0f);
    const auto mScale = modelMatrix(sc);
    check(vecNear(transformPoint(mScale, glm::vec3(1.0f, 1.0f, 1.0f)),
                  glm::vec3(2.0f, 3.0f, 4.0f)),
          "scale applied per-axis");

    // Rotation correctness is math, not visuals (assets are unlit/symmetric —
    // ADR-0010 Q3). Yaw about Y by +pi/2 must send +X to -Z (right-handed Y-up
    // convention, ADR-0006).
    Transform rotY;
    rotY.rotation.y = glm::pi<float>() * 0.5f;
    check(vecNear(transformPoint(modelMatrix(rotY), glm::vec3(1.0f, 0.0f, 0.0f)),
                  glm::vec3(0.0f, 0.0f, -1.0f)),
          "yaw +pi/2 maps +X -> -Z (YXZ rotation convention)");

    // Pitch about X by +pi/2 must send +Y to +Z.
    Transform rotX;
    rotX.rotation.x = glm::pi<float>() * 0.5f;
    check(vecNear(transformPoint(modelMatrix(rotX), glm::vec3(0.0f, 1.0f, 0.0f)),
                  glm::vec3(0.0f, 0.0f, 1.0f)),
          "pitch +pi/2 maps +Y -> +Z (YXZ convention)");

    // Composition order is exactly T·R·S — use a non-commuting fixture so a
    // future T·S·R regression is caught. point (1,0,0): scale -> (2,0,0),
    // rotate yaw -> (0,0,-2), translate -> (1,2,1). Under wrong order the
    // result would be (1,2,2).
    Transform comp;
    comp.position = glm::vec3(1.0f, 2.0f, 3.0f);
    comp.rotation.y = glm::pi<float>() * 0.5f;
    comp.scale = glm::vec3(2.0f, 1.0f, 1.0f);
    const auto mComp = modelMatrix(comp);
    check(vecNear(transformPoint(mComp, glm::vec3(1.0f, 0.0f, 0.0f)),
                  glm::vec3(1.0f, 2.0f, 1.0f)),
          "M == T·R·S for a non-commuting composition");
    const auto tTrs =
        glm::translate(glm::mat4(1.0f), comp.position) *
        (glm::eulerAngleYXZ(comp.rotation.y, comp.rotation.x, comp.rotation.z) *
         glm::scale(glm::mat4(1.0f), comp.scale));
    check(matNear(mComp, tTrs), "modelMatrix == T·(R·S) element-wise");
}

void testRestorePreservesEverything() {
    // A restored entity set must survive field-for-field, in the order given.
    // The ids are deliberately NOT ascending (7, 3, 11) so a sorting
    // implementation cannot pass by accident.
    noto3d::scene::Entity a;
    a.id      = 7;
    a.name    = "alpha";
    a.assetId = "smile";
    a.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    a.transform.rotation = glm::vec3(0.1f, 0.2f, 0.3f);
    a.transform.scale    = glm::vec3(4.0f, 5.0f, 6.0f);

    noto3d::scene::Entity b;
    b.id      = 3;
    b.name    = "beta";
    b.assetId = "crown";
    b.transform.position = glm::vec3(-1.0f, 0.5f, 2.5f);

    noto3d::scene::Entity c;
    c.id      = 11;
    c.name    = "gamma";
    c.assetId = "sun";

    auto restored = Scene::fromEntities({a, b, c});

    const auto es = restored.entities();
    check(es.size() == 3, "restore: entity count preserved");
    check(es.size() == 3 && es[0].id == 7 && es[1].id == 3 && es[2].id == 11,
          "restore: ids preserved in the given order (not sorted)");
    check(es.size() == 3 && es[0].name == "alpha" && es[1].name == "beta" &&
              es[2].name == "gamma",
          "restore: names preserved");
    check(es.size() == 3 && es[0].assetId == "smile" && es[1].assetId == "crown" &&
              es[2].assetId == "sun",
          "restore: assetIds preserved");
    check(es.size() == 3 && vecNear(es[0].transform.position, a.transform.position) &&
              vecNear(es[0].transform.rotation, a.transform.rotation) &&
              vecNear(es[0].transform.scale, a.transform.scale) &&
              vecNear(es[1].transform.position, b.transform.position) &&
              vecNear(es[2].transform.position, c.transform.position),
          "restore: transforms preserved field-for-field");
    check(es.size() == 3 && matNear(modelMatrix(es[0].transform),
                                   modelMatrix(a.transform)),
          "restore: modelMatrix of a restored entity is unchanged");
    check(es.size() == 3 && restored.findEntity(3) != nullptr &&
              restored.findEntity(3)->name == "beta",
          "restore: findEntity resolves a restored id");
}

void testRestoreDerivesAllocator() {
    // nextEntityId = max(restored ids) + 1, for ANY input order.
    noto3d::scene::Entity a; a.id = 4;
    noto3d::scene::Entity b; b.id = 2;
    noto3d::scene::Entity c; c.id = 9;
    auto restored = Scene::fromEntities({a, b, c});
    check(restored.createEntity("next", "warm") == 10,
          "derived allocator = max(restored ids) + 1 = 10");

    // An empty set leaves the first id at 1.
    auto empty = Scene::fromEntities({});
    check(empty.entities().empty(), "restore: empty set stays empty");
    check(empty.createEntity("first", "smile") == 1,
          "derived allocator for an empty set = 1");

    // Single entity, and a set whose max id is the FIRST element.
    noto3d::scene::Entity only; only.id = 5;
    check(Scene::fromEntities({only}).createEntity("x", "y") == 6,
          "derived allocator for a single entity = id + 1");
    noto3d::scene::Entity lo; lo.id = 1;
    noto3d::scene::Entity hi; hi.id = 30;
    check(Scene::fromEntities({hi, lo}).createEntity("x", "y") == 31,
          "derived allocator is the max, not the last element");
}

void testRestoreQ1ReusesDestroyedId() {
    // ADR-0013 Q1, pinned: a restore starts a NEW Scene lifetime, so the
    // allocator is derived from the surviving set and a previously destroyed
    // high-water id MAY reappear.  This is a decision, not drift: if this
    // assertion ever changes, ADR-0013 Q1 must be amended, not the test.
    Scene live;
    live.createEntity("a", "smile");   // id 1
    live.createEntity("b", "crown");   // id 2
    live.createEntity("c", "sun");     // id 3
    const auto preDestroyMax = 3u;
    check(live.destroyEntity(preDestroyMax), "Q1: the highest id is destroyed");

    std::vector<noto3d::scene::Entity> survivors(live.entities().begin(),
                                                live.entities().end());
    auto restored = Scene::fromEntities(std::move(survivors));

    check(restored.entities().size() == 2, "Q1: only the survivors were restored");
    // survivors are {1, 2}, so max(restored) + 1 == 3 — which IS the id that was
    // just destroyed.  That reuse is the accepted consequence of *deriving* the
    // counter from the entity set (ADR-0013 Q1): the file stays a pure function
    // of the ids it contains, and a new Scene lifetime begins at the load.
    const auto reused = restored.createEntity("d", "moon");
    check(reused == preDestroyMax,
          "Q1: the derived allocator reuses the destroyed high-water id (3)");
    check(reused == 3 && restored.findEntity(3) != nullptr,
          "Q1: the reused id resolves in the restored scene");
    check(restored.createEntity("e", "moon") == 4,
          "Q1: the allocator keeps advancing monotonically from there");
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-scene-tests");

    testIdentityAndStorage();
    testClearAndCounter();
    testEntityDefaults();
    testRestorePreservesEverything();
    testRestoreDerivesAllocator();
    testRestoreQ1ReusesDestroyedId();
    testModelMatrix();

    if (g_failures == 0) {
        NOTO3D_INFO("ALL PASS");
        return EXIT_SUCCESS;
    }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}