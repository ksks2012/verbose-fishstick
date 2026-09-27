#include "camera.hpp"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace noto3d::renderer {

namespace {

// Orthonormal camera basis derived from the spherical state.
// Shared by viewMatrix() and pan().
struct Basis {
    glm::vec3 eye;
    glm::vec3 right;
    glm::vec3 up;
};

Basis computeBasis(const glm::vec3& target,
                   float            yaw,
                   float            pitch,
                   float            distance) {
    const glm::vec3 offset{
        std::cos(pitch) * std::sin(yaw) * distance,
        std::sin(pitch) * distance,
        std::cos(pitch) * std::cos(yaw) * distance
    };
    const glm::vec3 eye     = target + offset;
    const glm::vec3 worldUp = {0.0f, 1.0f, 0.0f};
    const glm::vec3 fwd     = glm::normalize(target - eye);
    const glm::vec3 right   = glm::normalize(glm::cross(fwd, worldUp));
    const glm::vec3 up      = glm::cross(right, fwd);
    return {eye, right, up};
}

}  // namespace

Camera::Camera()
    : target_{0.0f, 0.0f, 0.0f}
    , yaw_{0.0f}
    , pitch_{std::asin(0.5f / std::sqrt(9.25f))}
    , distance_{std::sqrt(9.25f)}
{}

glm::mat4 Camera::viewMatrix() const {
    const auto b = computeBasis(target_, yaw_, pitch_, distance_);
    return glm::lookAt(b.eye, target_, b.up);
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
}

void Camera::orbit(float dx, float dy) noexcept {
    yaw_   += dx;
    pitch_ += dy;
    const float kLimit = glm::radians(89.0f);
    pitch_ = glm::clamp(pitch_, -kLimit, kLimit);
}

void Camera::zoom(float delta) noexcept {
    distance_ = glm::clamp(distance_ - delta, 0.5f, 20.0f);
}

void Camera::pan(float dx, float dy) noexcept {
    const auto b = computeBasis(target_, yaw_, pitch_, distance_);
    target_ += b.right * dx + b.up * dy;
}

glm::vec3 Camera::eye() const noexcept {
    return target_ + glm::vec3{
        std::cos(pitch_) * std::sin(yaw_) * distance_,
        std::sin(pitch_) * distance_,
        std::cos(pitch_) * std::cos(yaw_) * distance_
    };
}

}  // namespace noto3d::renderer
