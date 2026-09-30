#include "scene_io.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

#include "scene.hpp"

namespace noto3d::serialization {

const int kSceneFormatVersion = 1;

namespace {

using Json = nlohmann::ordered_json;

constexpr const char* kDiagnosticPrefix = "scene_io: ";

void addDiagnostic(LoadResult& result, std::string message) {
    result.diagnostics.push_back(kDiagnosticPrefix + std::move(message));
}

// The two guard shapes below recur across every field readEntity validates, so
// they are named once here: "is the key there at all" and "is it the right type"
// are different user mistakes needing different fixes, and 0043 shipped them as
// copy-paste per field.  Both write the SAME wording a hand-written guard would,
// because a diagnostic is a user-facing contract -- not something to "improve"
// while refactoring.
bool requireField(const Json& j, const char* key, const std::string& where,
                  std::string& reason) {
    if (j.contains(key)) return true;
    reason = where + " is missing '" + key + "'";
    return false;
}

bool requireString(const Json& j, const char* key, const std::string& where,
                   std::string& reason) {
    if (j[key].is_string()) return true;
    reason = where + " field '" + key + "' must be a string";
    return false;
}

bool requireInteger(const Json& j, const char* key, const std::string& where,
                    std::string& reason) {
    if (j[key].is_number_integer()) return true;
    reason = where + " field '" + key + "' must be an integer";
    return false;
}

// Read a 3-component vector.  On failure `reason` explains which of the three
// distinct defects occurred (Q3 message quality: a hand-editor must be told
// whether the key is absent, the wrong shape, or a bad component).
bool readVec3(const Json& parent, const char* key, glm::vec3& out, std::string& reason) {
    if (!parent.contains(key)) {
        reason = std::string("transform is missing '") + key + "'";
        return false;
    }
    const auto& a = parent[key];
    if (!a.is_array()) {
        reason = std::string("transform.") + key + " must be an array of 3 numbers";
        return false;
    }
    if (a.size() != 3) {
        reason = std::string("transform.") + key + " must have exactly 3 components, got " +
                 std::to_string(a.size());
        return false;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        if (!a[i].is_number()) {
            reason = std::string("transform.") + key + "[" + std::to_string(i) +
                     "] must be a number";
            return false;
        }
    }
    out = glm::vec3(a[0].get<float>(), a[1].get<float>(), a[2].get<float>());
    return true;
}

bool readTransform(const Json& parent, scene::Transform& out, std::string& reason) {
    if (!parent.is_object()) {
        reason = "'transform' must be an object";
        return false;
    }
    if (!readVec3(parent, "position", out.position, reason)) return false;
    if (!readVec3(parent, "rotation", out.rotation, reason)) return false;
    if (!readVec3(parent, "scale", out.scale, reason)) return false;
    return true;
}

// `index` is the entity's position in the document's entities array, so the
// diagnostic can point at it.  On failure `reason` names the field and what was
// expected -- never a generic "malformed entity entry".
bool readEntity(const Json& j, std::size_t index, scene::Entity& out, std::string& reason) {
    const std::string where = "entities[" + std::to_string(index) + "]";
    if (!j.is_object()) {
        reason = where + " must be an object";
        return false;
    }
    if (!requireField(j, "id", where, reason)) return false;
    // Read the id as int64 BEFORE narrowing: EntityId is uint32_t, so a negative
    // id in the file would wrap to a huge unsigned value and pass a naive
    // post-cast check.  The two rejections below are not equally motivated, and
    // the difference matters: id == 0 is InvalidEntityId and would trip
    // Scene::fromEntities' debug-only precondition (an ABORT, see the duplicate
    // check below), whereas a negative or fractional id is purely a malformed
    // value that would otherwise produce a nonsense identity.
    if (!requireInteger(j, "id", where, reason)) return false;
    const auto id = j["id"].get<std::int64_t>();
    if (id == 0) {
        reason = where + " field 'id' is 0, which is the reserved InvalidEntityId";
        return false;
    }
    if (id < 0) {
        reason = where + " field 'id' must be a positive integer, got " + std::to_string(id);
        return false;
    }
    // ...and so would an id ABOVE EntityId's range.  The cast below is unchecked,
    // so 2^32 narrows to 0 -- the reserved InvalidEntityId -- which under NDEBUG
    // loads silently with the one id the format forbids, and in a debug build
    // aborts inside Scene::fromEntities.  Both outcomes break the ticket's
    // contract that every structural defect returns a diagnostic instead.
    if (id > static_cast<std::int64_t>(std::numeric_limits<scene::EntityId>::max())) {
        reason = where + " field 'id' must be at most " +
                 std::to_string(std::numeric_limits<scene::EntityId>::max()) + ", got " +
                 std::to_string(id);
        return false;
    }
    if (!requireField(j, "name", where, reason)) return false;
    // NOTE: nlohmann's get<std::string>() on a NUMBER succeeds (it stringifies),
    // so is_string() is load-bearing here, not decoration -- without it a
    // "name": 5 document loads silently with name == "5".
    if (!requireString(j, "name", where, reason)) return false;
    if (!requireField(j, "assetId", where, reason)) return false;
    // An EMPTY assetId passes: present-and-a-string is the only structural rule.
    // The serializer never resolves an assetId (the 0041 guard makes consulting
    // the registry impossible), so it has no standing to judge resolvability --
    // and an id it cannot resolve is retained verbatim for the M4 resolution
    // seam to skip at draw time (ADR-0013 Q3 lenient references).
    if (!requireString(j, "assetId", where, reason)) return false;
    // The vectors live under the nested "transform" key, so hand THAT object to
    // readTransform, not the entity: readTransform looks for position/rotation/
    // scale as direct children of its argument.
    if (!requireField(j, "transform", where, reason)) return false;
    if (!readTransform(j["transform"], out.transform, reason)) {
        // Two steps, NOT `reason = where + ": " + reason`: that one reads and
        // writes the same object in a single expression and only happens to work
        // because operator+ finishes building the temporary first.
        const std::string inner = reason;
        reason = where + ": " + inner;
        return false;
    }

    out.id = static_cast<scene::EntityId>(id);
    out.name = j["name"].get<std::string>();
    out.assetId = j["assetId"].get<std::string>();
    return true;
}

Json vec3ToJson(const glm::vec3& v) {
    return Json::array({v.x, v.y, v.z});
}

Json transformToJson(const scene::Transform& t) {
    Json j = Json::object();
    j["position"] = vec3ToJson(t.position);
    j["rotation"] = vec3ToJson(t.rotation);
    j["scale"] = vec3ToJson(t.scale);
    return j;
}

Json entityToJson(const scene::Entity& e) {
    Json j = Json::object();
    j["id"] = e.id;
    j["name"] = e.name;
    j["assetId"] = e.assetId;
    j["transform"] = transformToJson(e.transform);
    return j;
}

}  // namespace

std::string serializeScene(const scene::Scene& scene) {
    Json j = Json::object();
    j["version"] = kSceneFormatVersion;
    j["entities"] = Json::array();
    for (const auto& e : scene.entities()) {
        j["entities"].push_back(entityToJson(e));
    }
    return j.dump();
}

LoadResult deserializeScene(std::string_view text) {
    LoadResult result;

    Json j;
    try {
        j = Json::parse(text);
    } catch (const nlohmann::json::exception& e) {
        addDiagnostic(result, std::string("malformed JSON: ") + e.what());
        return result;
    }

    if (!j.is_object()) {
        addDiagnostic(result, "document root is not a JSON object");
        return result;
    }

    // Version gate (Q4): accept only the integer 1.  Q4 requires the diagnostic
    // to DISTINGUISH an unsupported version from a malformed one, so the three
    // shapes get three messages rather than one shared string: a user who typo'd
    // "version" needs a different fix than one who is running a v2 file.
    if (!j.contains("version")) {
        addDiagnostic(result, "missing 'version' field (expected integer " +
                                 std::to_string(kSceneFormatVersion) + ")");
        return result;
    }
    if (!j["version"].is_number_integer()) {
        addDiagnostic(result, "malformed 'version': not an integer");
        return result;
    }
    const auto version = j["version"].get<std::int64_t>();
    if (version != kSceneFormatVersion) {
        addDiagnostic(result, "unsupported scene version: " + std::to_string(version) +
                                 " (expected " + std::to_string(kSceneFormatVersion) + ")");
        return result;
    }

    if (!j.contains("entities")) {
        addDiagnostic(result, "missing 'entities' field");
        return result;
    }
    if (!j["entities"].is_array()) {
        addDiagnostic(result, "'entities' must be an array");
        return result;
    }

    std::vector<scene::Entity> entities;
    entities.reserve(j["entities"].size());
    // Uniqueness is checked HERE, not deferred to 0044, for a concrete reason:
    // Scene::fromEntities asserts uniqueness as a debug-only precondition, so a
    // document with duplicate ids would ABORT the process rather than return a
    // diagnostic.  An abort is strictly worse than a throw and would break this
    // function's "never throws, never partially-populated" contract, so the
    // serializer must never hand fromEntities data it cannot accept.  0044 still
    // owns the richer diagnostics (which entity, which id) and the
    // unresolvable-reference contract.
    std::unordered_set<scene::EntityId> seen;
    for (std::size_t i = 0; i < j["entities"].size(); ++i) {
        const auto& ej = j["entities"][i];
        scene::Entity e;
        std::string reason;
        if (!readEntity(ej, i, e, reason)) {
            addDiagnostic(result, reason);
            return result;
        }
        if (!seen.insert(e.id).second) {
            addDiagnostic(result, "entities[" + std::to_string(i) +
                                     "] duplicates entity id " + std::to_string(e.id) +
                                     " (ids must be unique)");
            return result;
        }
        entities.push_back(std::move(e));
    }

    // Ticket-0042 restore primitive derives the allocator (max + 1, empty -> 1).
    // No counter is ever trusted from the file.
    result.scene.emplace(scene::Scene::fromEntities(std::move(entities)));
    return result;
}

}  // namespace noto3d::serialization
