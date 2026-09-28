#include "transform_edit.hpp"
#include <log.hpp>

namespace noto3d::app {

// ---------------------------------------------------------------------------
// Component-wise conversions
// ---------------------------------------------------------------------------

glm::vec3 rotationDegreesToRadians(glm::vec3 deg) noexcept {
    return {degreesToRadians(deg.x), degreesToRadians(deg.y), degreesToRadians(deg.z)};
}

glm::vec3 rotationRadiansToDegrees(glm::vec3 rad) noexcept {
    return {radiansToDegrees(rad.x), radiansToDegrees(rad.y), radiansToDegrees(rad.z)};
}

// ---------------------------------------------------------------------------
// Commit functions (mutation + structured log)
// ---------------------------------------------------------------------------

void commitPosition(Editor& editor, scene::EntityId id, glm::vec3 newPos) {
    editor.setEntityPosition(id, newPos);
    const auto name = editor.entityName(id);
    NOTO3D_INFO("M5_XFRM entity='{}' id={} position x={:.3f} y={:.3f} z={:.3f}",
                name ? *name : "???", static_cast<unsigned>(id),
                newPos.x, newPos.y, newPos.z);
}

void commitRotation(Editor& editor, scene::EntityId id, glm::vec3 pitchYawRollDeg) {
    // Convert display degrees → stored radians (ADR-0011 Q13; Scene stores radians)
    const glm::vec3 rad = rotationDegreesToRadians(pitchYawRollDeg);
    editor.setEntityRotation(id, rad);
    const auto name = editor.entityName(id);
    // Log both units explicitly so the evidence matches what the user saw (Q8)
    NOTO3D_INFO(
        "M5_XFRM entity='{}' id={} rotation "
        "pitch={:.3f}deg({:.4f}rad) yaw={:.3f}deg({:.4f}rad) roll={:.3f}deg({:.4f}rad)",
        name ? *name : "???", static_cast<unsigned>(id),
        pitchYawRollDeg.x, rad.x,
        pitchYawRollDeg.y, rad.y,
        pitchYawRollDeg.z, rad.z);
}

void commitScale(Editor& editor, scene::EntityId id, glm::vec3 newScale) {
    editor.setEntityScale(id, newScale);
    const auto name = editor.entityName(id);
    NOTO3D_INFO("M5_XFRM entity='{}' id={} scale x={:.3f} y={:.3f} z={:.3f}",
                name ? *name : "???", static_cast<unsigned>(id),
                newScale.x, newScale.y, newScale.z);
}

}  // namespace noto3d::app
