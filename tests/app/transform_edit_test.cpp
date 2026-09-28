#include "transform_edit.hpp"
#include "log.hpp"

#include <cmath>
#include <cstdlib>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

bool near(float a, float b, float tol = 1e-4f) {
    return std::abs(a - b) < tol;
}

bool nearVec(const glm::vec3& a, const glm::vec3& b, float tol = 1e-4f) {
    return near(a.x, b.x, tol) && near(a.y, b.y, tol) && near(a.z, b.z, tol);
}

using namespace noto3d::app;
using namespace noto3d::scene;

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-transform-edit-tests");

    // ── degreesToRadians ────────────────────────────────────────────────────
    check(near(degreesToRadians(0.0f),    0.0f),             "0 deg -> 0 rad");
    check(near(degreesToRadians(90.0f),   3.14159265f / 2),  "90 deg -> pi/2");
    check(near(degreesToRadians(180.0f),  3.14159265f),       "180 deg -> pi");
    check(near(degreesToRadians(360.0f),  2 * 3.14159265f),  "360 deg -> 2pi");
    check(near(degreesToRadians(-90.0f), -3.14159265f / 2),  "-90 deg -> -pi/2");
    check(near(degreesToRadians(270.0f),  3 * 3.14159265f / 2), "270 deg -> 3pi/2");

    // ── radiansToDegrees ────────────────────────────────────────────────────
    check(near(radiansToDegrees(0.0f),           0.0f),   "0 rad -> 0 deg");
    check(near(radiansToDegrees(3.14159265f / 2), 90.0f), "pi/2 -> 90 deg");
    check(near(radiansToDegrees(3.14159265f),    180.0f), "pi -> 180 deg");
    check(near(radiansToDegrees(-3.14159265f),  -180.0f), "-pi -> -180 deg");
    check(near(radiansToDegrees(2 * 3.14159265f), 360.0f),"2pi -> 360 deg");

    // ── roundtrip ───────────────────────────────────────────────────────────
    for (const float v : {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, -45.0f, -180.0f, 270.0f, 360.0f}) {
        check(near(radiansToDegrees(degreesToRadians(v)), v, 1e-3f),
              ("roundtrip deg->rad->deg: " + std::to_string(v)).c_str());
    }

    // ── rotationDegreesToRadians / rotationRadiansToDegrees ─────────────────
    {
        const glm::vec3 deg{30.0f, 90.0f, -45.0f};
        const glm::vec3 rad = rotationDegreesToRadians(deg);
        check(near(rad.x, degreesToRadians(30.0f)),  "rotDegreesToRad: pitch");
        check(near(rad.y, degreesToRadians(90.0f)),  "rotDegreesToRad: yaw");
        check(near(rad.z, degreesToRadians(-45.0f)), "rotDegreesToRad: roll");

        const glm::vec3 back = rotationRadiansToDegrees(rad);
        check(nearVec(back, deg), "rotRadToDeg: roundtrip");
    }

    // ── commitPosition ──────────────────────────────────────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("cube", "cube");
        ed.select(id);

        commitPosition(ed, id, {1.0f, 2.0f, 3.0f});

        const auto t = ed.selectedTransform();
        check(t.has_value(), "commitPosition: transform readable after commit");
        if (t) {
            check(near(t->position.x, 1.0f), "commitPosition: x");
            check(near(t->position.y, 2.0f), "commitPosition: y");
            check(near(t->position.z, 3.0f), "commitPosition: z");
            // rotation and scale unchanged
            check(near(t->rotation.x, 0.0f) && near(t->rotation.y, 0.0f),
                  "commitPosition: rotation untouched");
            check(near(t->scale.x, 1.0f) && near(t->scale.y, 1.0f),
                  "commitPosition: scale untouched");
        }
    }

    // ── commitRotation: degrees → radians, other fields preserved ───────────
    {
        Editor ed;
        const auto id = ed.createEntity("rotCube", "cube");
        ed.select(id);
        ed.setEntityPosition(id, {5.0f, 0.0f, 0.0f});  // set a non-default position

        // Rotate yaw=90 deg
        commitRotation(ed, id, {0.0f, 90.0f, 0.0f});

        const auto t = ed.selectedTransform();
        check(t.has_value(), "commitRotation: transform readable");
        if (t) {
            check(near(t->rotation.x, 0.0f),                  "commitRotation: pitch=0");
            check(near(t->rotation.y, degreesToRadians(90.0f)), "commitRotation: yaw=pi/2");
            check(near(t->rotation.z, 0.0f),                  "commitRotation: roll=0");
            // position and scale untouched
            check(near(t->position.x, 5.0f), "commitRotation: position preserved");
            check(near(t->scale.x, 1.0f),    "commitRotation: scale preserved");
        }
    }

    // ── commitRotation: negative degrees ───────────────────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("x", "x");
        ed.select(id);
        commitRotation(ed, id, {-45.0f, 0.0f, 0.0f});
        const auto t = ed.selectedTransform();
        check(t && near(t->rotation.x, degreesToRadians(-45.0f)),
              "commitRotation: negative pitch");
    }

    // ── commitRotation: past ±180° ──────────────────────────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("x", "x");
        ed.select(id);
        commitRotation(ed, id, {0.0f, 270.0f, 0.0f});
        const auto t = ed.selectedTransform();
        check(t && near(t->rotation.y, degreesToRadians(270.0f)),
              "commitRotation: yaw=270deg stored as 3pi/2 rad");
    }

    // ── commitScale ─────────────────────────────────────────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("s", "s");
        ed.select(id);
        ed.setEntityRotation(id, {0.1f, 0.2f, 0.3f});  // set rotation beforehand

        commitScale(ed, id, {2.0f, 3.0f, 4.0f});

        const auto t = ed.selectedTransform();
        check(t.has_value(), "commitScale: transform readable");
        if (t) {
            check(near(t->scale.x, 2.0f), "commitScale: x");
            check(near(t->scale.y, 3.0f), "commitScale: y");
            check(near(t->scale.z, 4.0f), "commitScale: z");
            // rotation untouched
            check(near(t->rotation.x, 0.1f) && near(t->rotation.y, 0.2f),
                  "commitScale: rotation preserved");
        }
    }

    // ── selectedTransform returns nullopt when nothing selected ──────────────
    {
        Editor ed;
        check(!ed.selectedTransform().has_value(),
              "selectedTransform: nullopt with no selection");
        const auto id = ed.createEntity("x", "x");
        check(!ed.selectedTransform().has_value(),
              "selectedTransform: nullopt before select");
        ed.select(id);
        check(ed.selectedTransform().has_value(),
              "selectedTransform: has value after select");
    }

    // ── entityName ──────────────────────────────────────────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("named", "asset");
        check(ed.entityName(id) && *ed.entityName(id) == "named",
              "entityName: correct");
        check(!ed.entityName(InvalidEntityId).has_value(),
              "entityName: nullopt for invalid id");
        ed.destroyEntity(id);
        check(!ed.entityName(id).has_value(),
              "entityName: nullopt after destroy");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
