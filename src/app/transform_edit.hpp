#pragma once

#include <editor.hpp>
#include <scene.hpp>

#include <glm/glm.hpp>

namespace noto3d::app {

// Degree ↔ radian conversion (ADR-0011 Q13).
//
// Degrees are a presentation-layer convention; the Scene stores radians.
// These functions are ImGui-free and headless-testable.  The unit conversion
// belongs in the app layer so the UI adapter never invents its own formula
// (same principle as pixel→radian mapping in camera_input.cpp; Q7/Q13).

[[nodiscard]] inline float degreesToRadians(float deg) noexcept {
    return deg * (3.14159265358979323846f / 180.0f);
}

[[nodiscard]] inline float radiansToDegrees(float rad) noexcept {
    return rad * (180.0f / 3.14159265358979323846f);
}

// Component-wise conversions for rotation vectors.
// Convention: vec3 is (pitch=x, yaw=y, roll=z), matching scene::Transform::rotation.
[[nodiscard]] glm::vec3 rotationDegreesToRadians(glm::vec3 deg) noexcept;
[[nodiscard]] glm::vec3 rotationRadiansToDegrees(glm::vec3 rad) noexcept;

// Commit functions — ImGui-free (no widget types in these signatures).
//
// Each function:
//   1. Calls the appropriate Editor mutation (EntityId-keyed, ADR-0011 Q3/Q11).
//   2. Logs the committed change with both units where applicable (Q8).
//
// These are the only code paths allowed to call editor.setEntityXxx().
// The UI adapter calls the relevant commit function from its
// IsItemDeactivatedAfterEdit() boundary — not from per-frame code (Q8).
//
// commitRotation: pitchYawRollDeg is in DEGREES [x=pitch, y=yaw, z=roll].
// Both the displayed (degrees) and stored (radians) values appear in the log.

void commitPosition(Editor& editor, scene::EntityId id, glm::vec3 newPos);
void commitRotation(Editor& editor, scene::EntityId id, glm::vec3 pitchYawRollDeg);
void commitScale   (Editor& editor, scene::EntityId id, glm::vec3 newScale);

}  // namespace noto3d::app
