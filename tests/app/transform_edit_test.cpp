#include "transform_edit.hpp"
#include "log.hpp"

#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

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

// Log-capture seam so the M5_XFRM dual-unit contract is ASSERTED, not just
// printed (ticket 0033: transform commits including the dual-unit line are
// covered by structured runtime-log assertions).
class CaptureSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    std::vector<std::string> lines;
    void clear() { lines.clear(); }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t buf;
        formatter_->format(msg, buf);
        lines.emplace_back(buf.data(), buf.size());
    }
    void flush_() override {}
};

bool logHas(const std::vector<std::string>& lines, const std::string& needle) {
    return std::any_of(lines.begin(), lines.end(),
                       [&](const std::string& l) {
                           return l.find(needle) != std::string::npos;
                       });
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-transform-edit-tests");

    // Attach capture sink (alongside the normal stdout sink) so structured-log
    // assertions can verify the M5_XFRM format contract (ticket 0033).
    std::shared_ptr<CaptureSink> capture = std::make_shared<CaptureSink>();
    spdlog::default_logger()->sinks().push_back(capture);

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

    // ── M5_XFRM dual-unit log format contract (ticket 0033) ─────────────────
    // The commit functions must log BOTH display units (degrees) AND stored
    // units (radians) so the log evidence matches what the user saw (ADR-0011 Q8/Q13).
    {
        capture->clear();
        Editor ed;
        const auto id = ed.createEntity("logTest", "cube");
        ed.select(id);

        // commitRotation: yaw=90 deg → emits M5_XFRM with both units
        commitRotation(ed, id, {0.0f, 90.0f, 0.0f});

        // The line must carry "M5_XFRM" marker
        check(logHas(capture->lines, "M5_XFRM"),
              "M5_XFRM: log line present on rotation commit");
        // "entity='logTest'" — entity name in log
        check(logHas(capture->lines, "entity='logTest'"),
              "M5_XFRM: entity name present in log");
        // Dual-unit format: display value "90.000deg" AND stored value "(1.5708rad)"
        check(logHas(capture->lines, "90.000deg"),
              "M5_XFRM: displayed degrees present");
        check(logHas(capture->lines, "1.5708rad"),
              "M5_XFRM: stored radians present (dual-unit, ADR-0011 Q13)");

        // commitPosition: no degree conversion, just XYZ
        capture->clear();
        commitPosition(ed, id, {1.0f, 2.0f, 3.0f});
        check(logHas(capture->lines, "M5_XFRM"),   "M5_XFRM: position commit logged");
        check(logHas(capture->lines, "position"),   "M5_XFRM: 'position' keyword");
        check(logHas(capture->lines, "x=1.000"),    "M5_XFRM: position x value");

        // commitScale
        capture->clear();
        commitScale(ed, id, {2.0f, 2.0f, 2.0f});
        check(logHas(capture->lines, "M5_XFRM"),   "M5_XFRM: scale commit logged");
        check(logHas(capture->lines, "scale"),      "M5_XFRM: 'scale' keyword");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
