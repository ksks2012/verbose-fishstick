#pragma once

#include <glm/glm.hpp>

namespace noto3d::renderer {

// Orbit/zoom/pan camera — pure math, no GL/GLFW/Scene dependency (ADR-0010 Q7).
//
// Spherical convention (locked):
//   offset.x = cos(pitch) * sin(yaw) * distance
//   offset.y = sin(pitch) * distance
//   offset.z = cos(pitch) * cos(yaw) * distance
//   eye     = target + offset          (worldUp = (0,1,0))
//   forward = normalize(target - eye)
//   right   = normalize(cross(forward, worldUp))
//   up      = cross(right, forward)
//
// Initial state (locked):
//   target=(0,0,0), yaw=0,
//   distance=sqrt(9.25), pitch=asin(0.5/sqrt(9.25))
//   => eye exactly (0, 0.5, 3)  =  M3 camera position.
//
// No mutable-state exposure: public float& accessors are forbidden.
class Camera {
public:
    Camera();

    // View matrix: lookAt(eye, target, up) via the spherical convention above.
    // At default state equals glm::lookAt({0,0.5,3},{0,0,0},{0,1,0}).
    [[nodiscard]] glm::mat4 viewMatrix() const;

    // Perspective projection: FOV 45°, near 0.1, far 100.
    // Recompute each frame with the current framebuffer aspect ratio.
    [[nodiscard]] glm::mat4 projectionMatrix(float aspect) const;

    // orbit(dx, dy): yaw += dx; pitch += dy; pitch clamped to ±89°.
    // Sign contract (locked): yaw grows with dx, pitch grows with dy.
    void orbit(float dx, float dy) noexcept;

    // zoom(delta): distance -= delta, clamped to [0.5, 20.0].
    // Positive delta zooms in (distance decreases); FOV is unchanged.
    void zoom(float delta) noexcept;

    // pan(dx, dy): target += right*dx + up*dy (world-space displacement along
    // camera-local axes, NOT world X/Y).  right and up are the orthonormal basis
    // derived from the current spherical state.  Only target is mutated;
    // yaw/pitch/distance remain unchanged so subsequent orbit/zoom are consistent.
    void pan(float dx, float dy) noexcept;

    // Const getters — no mutable references exposed.
    [[nodiscard]] const glm::vec3& target()   const noexcept { return target_; }
    [[nodiscard]] float            yaw()      const noexcept { return yaw_; }
    [[nodiscard]] float            pitch()    const noexcept { return pitch_; }
    [[nodiscard]] float            distance() const noexcept { return distance_; }
    [[nodiscard]] glm::vec3        eye()      const noexcept;

private:
    glm::vec3 target_;
    float     yaw_;
    float     pitch_;
    float     distance_;
};

}  // namespace noto3d::renderer
