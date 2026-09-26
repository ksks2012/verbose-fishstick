#include "scene.hpp"
#include "log.hpp"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

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

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-scene-tests");

    testIdentityAndStorage();
    testClearAndCounter();
    testEntityDefaults();
    testModelMatrix();

    if (g_failures == 0) {
        NOTO3D_INFO("ALL PASS");
        return EXIT_SUCCESS;
    }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}