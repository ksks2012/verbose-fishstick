#include "camera.hpp"
#include "log.hpp"

#include <cmath>
#include <cstdlib>

#include <glm/gtc/matrix_transform.hpp>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

bool near(float a, float b, float tol = 1e-4f) {
    return std::abs(a - b) < tol;
}

bool nearVec3(const glm::vec3& a, const glm::vec3& b, float tol = 1e-4f) {
    return near(a.x, b.x, tol) && near(a.y, b.y, tol) && near(a.z, b.z, tol);
}

bool nearMat4(const glm::mat4& a, const glm::mat4& b, float tol = 1e-4f) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!near(a[c][r], b[c][r], tol)) return false;
    return true;
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-camera-tests");
    using noto3d::renderer::Camera;

    // ── Test 1: default viewMatrix == M3 lookAt ──────────────────────────────
    {
        const Camera cam;
        const auto got = cam.viewMatrix();
        const auto expected = glm::lookAt(glm::vec3{0.0f, 0.5f, 3.0f},
                                          glm::vec3{0.0f, 0.0f, 0.0f},
                                          glm::vec3{0.0f, 1.0f, 0.0f});
        check(nearMat4(got, expected),
              "initial view == M3 lookAt((0,0.5,3),(0,0,0),(0,1,0))");
    }

    // ── Test 2: initial eye position ────────────────────────────────────────
    {
        const Camera cam;
        check(nearVec3(cam.eye(), {0.0f, 0.5f, 3.0f}),
              "initial eye == (0, 0.5, 3)");
    }

    // ── Test 3: initial primitive state ─────────────────────────────────────
    {
        const Camera   cam;
        const float kDist  = std::sqrt(9.25f);
        const float kPitch = std::asin(0.5f / kDist);
        check(nearVec3(cam.target(), {0.0f, 0.0f, 0.0f}), "initial target == (0,0,0)");
        check(near(cam.yaw(),      0.0f),   "initial yaw == 0");
        check(near(cam.pitch(),  kPitch),   "initial pitch == asin(0.5/sqrt(9.25))");
        check(near(cam.distance(), kDist),  "initial distance == sqrt(9.25)");
    }

    // ── Test 4: projectionMatrix equals glm::perspective(45°, aspect, 0.1, 100) ──
    {
        const Camera cam;
        const float  a = 16.0f / 9.0f;
        const auto got      = cam.projectionMatrix(a);
        const auto expected = glm::perspective(glm::radians(45.0f), a, 0.1f, 100.0f);
        check(nearMat4(got, expected),
              "projectionMatrix(16/9) == perspective(45°,16/9,0.1,100)");
    }

    // ── Test 5: orbit +dx increases yaw ─────────────────────────────────────
    {
        Camera cam;
        const float y0 = cam.yaw();
        cam.orbit(0.1f, 0.0f);
        check(cam.yaw() > y0,               "orbit(+dx,0): yaw increases");
        check(near(cam.yaw(), y0 + 0.1f),   "orbit(+dx,0): yaw == y0+dx");
    }

    // ── Test 6: orbit +dy increases pitch ────────────────────────────────────
    {
        Camera cam;
        const float p0 = cam.pitch();
        cam.orbit(0.0f, 0.1f);
        check(cam.pitch() > p0,             "orbit(0,+dy): pitch increases");
        check(near(cam.pitch(), p0 + 0.1f), "orbit(0,+dy): pitch == p0+dy");
    }

    // ── Test 7: pitch clamped at ±89° ────────────────────────────────────────
    {
        Camera cam;
        cam.orbit(0.0f, 1000.0f);
        check(near(cam.pitch(), glm::radians(89.0f)),
              "pitch clamp: +∞ → +89°");
        cam.orbit(0.0f, -2000.0f);
        check(near(cam.pitch(), glm::radians(-89.0f)),
              "pitch clamp: -∞ → -89°");
    }

    // ── Test 8: zoom +delta decreases distance ───────────────────────────────
    {
        Camera cam;
        const float d0 = cam.distance();
        cam.zoom(0.5f);
        check(cam.distance() < d0,             "zoom(+delta): distance decreases");
        check(near(cam.distance(), d0 - 0.5f), "zoom(+delta): distance == d0-delta");
    }

    // ── Test 9: zoom clamp [0.5, 20] ────────────────────────────────────────
    {
        Camera cam;
        cam.zoom(1000.0f);
        check(near(cam.distance(), 0.5f),  "zoom clamp: min == 0.5");
        cam.zoom(-1000.0f);
        check(near(cam.distance(), 20.0f), "zoom clamp: max == 20.0");
    }

    // ── Test 10: zoom does not change projectionMatrix (FOV unchanged) ───────
    {
        Camera cam;
        const auto projBefore = cam.projectionMatrix(1.0f);
        cam.zoom(1.0f);
        const auto projAfter  = cam.projectionMatrix(1.0f);
        check(nearMat4(projBefore, projAfter),
              "zoom: projectionMatrix (FOV) unchanged");
    }

    // ── Test 11: pan moves target; yaw/pitch/distance unchanged ─────────────
    {
        Camera cam;
        const glm::vec3 t0 = cam.target();
        cam.pan(1.0f, 0.0f);
        check(!nearVec3(cam.target(), t0),           "pan(1,0): target moved");
        check(near(cam.yaw(),      0.0f),             "pan: yaw unchanged");
        check(near(cam.pitch(),    std::asin(0.5f / std::sqrt(9.25f))),
              "pan: pitch unchanged");
        check(near(cam.distance(), std::sqrt(9.25f)), "pan: distance unchanged");
    }

    // ── Test 12: pan dx along camera-local right (at yaw=0, right≈world +X) ─
    {
        // At yaw=0, cross(forward, worldUp) = (3/sqrt(9.25), 0, 0) → right=(1,0,0).
        Camera cam;
        const glm::vec3 t0 = cam.target();
        cam.pan(1.0f, 0.0f);
        check(near(cam.target().x, t0.x + 1.0f, 1e-3f),
              "pan(1,0): target.x += 1 (right ≈ world +X at yaw=0)");
        check(near(cam.target().z, t0.z,         1e-3f),
              "pan(1,0): target.z unchanged at yaw=0");
    }

    // ── Test 13: pan dy along camera-local up (NOT world Y) ─────────────────
    {
        // At initial state: up = (0, 3/sqrt(9.25), -0.5/sqrt(9.25)).
        // pan(0,1) → target.z becomes non-zero, confirming up ≠ worldUp.
        const float k     = std::sqrt(9.25f);
        const float expUy =  3.0f / k;
        const float expUz = -0.5f / k;
        Camera cam;
        const glm::vec3 t0 = cam.target();
        cam.pan(0.0f, 1.0f);
        check(near(cam.target().y, t0.y + expUy, 1e-3f),
              "pan(0,1): target.y correct (camera-local up.y)");
        check(near(cam.target().z, t0.z + expUz, 1e-3f),
              "pan(0,1): target.z ≠ 0 confirms up ≠ worldUp");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
