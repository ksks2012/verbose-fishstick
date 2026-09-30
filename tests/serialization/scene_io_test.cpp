#include "scene_io.hpp"
#include "log.hpp"

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "scene.hpp"

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

using noto3d::scene::Entity;
using noto3d::scene::EntityId;
using noto3d::scene::Scene;
using noto3d::scene::Transform;
using noto3d::serialization::LoadResult;

void setTransform(Entity& e, float px, float py, float pz, float rx, float ry, float rz,
                  float sx, float sy, float sz) {
    e.transform.position = glm::vec3(px, py, pz);
    e.transform.rotation = glm::vec3(rx, ry, rz);
    e.transform.scale = glm::vec3(sx, sy, sz);
}

// The central test: a real round-trip, build -> write -> read -> compare.
void testRoundTrip() {
    Scene scene;
    const EntityId id1 = scene.createEntity("cube", "noto_cube");
    const EntityId id2 = scene.createEntity("sphere", "noto_sphere");
    Entity& e1 = *scene.findEntity(id1);
    Entity& e2 = *scene.findEntity(id2);
    setTransform(e1, 1.5f, -2.25f, 0.125f, 0.5f, -0.75f, 1.0f, 2.0f, 0.5f, 1.0f);
    setTransform(e2, -100.5f, 0.1f, 3.75f, -1.25f, 2.5f, 0.0f, 0.25f, 4.0f, 0.125f);

    const std::string text = noto3d::serialization::serializeScene(scene);
    LoadResult r = noto3d::serialization::deserializeScene(text);
    check(r.scene.has_value(), "round-trip: scene engaged");
    check(r.diagnostics.empty(), "round-trip: no diagnostics");
    if (!r.scene.has_value()) return;

    check(r.scene->entities().size() == 2, "round-trip: entity count");
    if (r.scene->entities().size() != 2) return;

    for (int i = 0; i < 2; ++i) {
        const Entity& a = scene.entities()[i];
        const Entity& b = r.scene->entities()[i];
        char name[128];
        std::snprintf(name, sizeof(name), "round-trip entity %d: id/name/assetId exact", i);
        check(a.id == b.id && a.name == b.name && a.assetId == b.assetId, name);
        std::snprintf(name, sizeof(name),
                      "round-trip entity %d: transform float-exact (==)", i);
        check(a.transform.position == b.transform.position &&
                  a.transform.rotation == b.transform.rotation &&
                  a.transform.scale == b.transform.scale,
              name);
    }
}

// Q1: reuse pinned through the real save/load path. The destroy step makes the
// assertion non-trivial: without it, max(restored)+1 would differ from the
// destroyed value.
void testQ1ReuseThroughSaveLoad() {
    Scene scene;
    (void)scene.createEntity("a", "noto_cube");
    (void)scene.createEntity("b", "noto_cube");
    const EntityId c = scene.createEntity("c", "noto_cube");
    check(c == 3, "Q1: third id is 3");
    check(scene.destroyEntity(c), "Q1: destroy highest id");
    check(scene.entities().size() == 2, "Q1: two entities survive");

    const std::string text = noto3d::serialization::serializeScene(scene);
    LoadResult r = noto3d::serialization::deserializeScene(text);
    check(r.scene.has_value(), "Q1: load succeeds");
    if (!r.scene.has_value()) return;

    const EntityId reused = r.scene->createEntity("new", "noto_cube");
    check(reused == c, "Q1: new id equals the destroyed high-water value (reuse)");
}

// Q10: empty document is valid, yields nextEntityId == 1.
void testEmptyDocument() {
    LoadResult r =
        noto3d::serialization::deserializeScene(R"({"version":1,"entities":[]})");
    check(r.scene.has_value(), "Q10: empty document loads");
    check(r.diagnostics.empty(), "Q10: empty document has no diagnostics");
    if (!r.scene.has_value()) return;
    check(r.scene->entities().empty(), "Q10: empty scene has no entities");
    const EntityId e = r.scene->createEntity("first", "noto_cube");
    check(e == 1, "Q10: nextEntityId == 1 after empty load");
}

// Q4: unknown keys ignored at envelope and entity level.
void testUnknownKeysIgnored() {
    const char* doc = R"({
        "version": 1,
        "entities": [
            { "id": 1, "name": "n", "assetId": "a", "futureField": 42,
              "transform": { "position": [0,0,0], "rotation": [0,0,0],
                             "scale": [1,1,1], "extraVec": [9,9,9] } }
        ],
        "unknownEnvelopeKey": { "nested": true }
    })";
    LoadResult r = noto3d::serialization::deserializeScene(doc);
    check(r.scene.has_value(), "Q4: unknown envelope+entity keys load successfully");
    check(r.diagnostics.empty(), "Q4: unknown keys produce no diagnostics");
    if (!r.scene.has_value()) return;
    check(r.scene->entities().size() == 1, "Q4: one entity loaded");
    if (r.scene->entities().empty()) return;
    const Entity& e = r.scene->entities()[0];
    check(e.id == 1 && e.name == "n" && e.assetId == "a", "Q4: known fields read correctly");
}

// Q4: version gate. Missing / string / 2 are structural failures.
void testVersionGate() {
    const char* missing = R"({"entities":[]})";
    LoadResult r1 = noto3d::serialization::deserializeScene(missing);
    check(!r1.scene.has_value(), "Q4: missing version -> no scene");
    check(!r1.diagnostics.empty(), "Q4: missing version -> diagnostic present");

    const char* asString = R"({"version":"1","entities":[]})";
    LoadResult r2 = noto3d::serialization::deserializeScene(asString);
    check(!r2.scene.has_value(), "Q4: string version -> no scene");
    check(!r2.diagnostics.empty(), "Q4: string version -> diagnostic present");

    const char* wrong = R"({"version":2,"entities":[]})";
    LoadResult r3 = noto3d::serialization::deserializeScene(wrong);
    check(!r3.scene.has_value(), "Q4: version 2 -> no scene");
    check(!r3.diagnostics.empty(), "Q4: version 2 -> diagnostic present");
}

// Invariant: structural failure always yields nullopt AND >= 1 diagnostic.
void testStructuralFailureInvariants() {
    const char* cases[] = {
        R"(not json at all)",
        R"(42)",
        R"({"version":1,"entities":{}})",
        R"({"version":1,"entities":[{"name":"x","assetId":"y","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})",
    };
    const char* labels[] = {"malformed JSON", "non-object root", "non-array entities",
                            "entity missing id"};
    for (int i = 0; i < 4; ++i) {
        LoadResult r = noto3d::serialization::deserializeScene(cases[i]);
        char name[128];
        std::snprintf(name, sizeof(name), "structural invariant: %s -> nullopt", labels[i]);
        check(!r.scene.has_value(), name);
        std::snprintf(name, sizeof(name), "structural invariant: %s -> diagnostic", labels[i]);
        check(!r.diagnostics.empty(), name);
    }
}

// Every case here is one a malformed document can actually produce. Each must
// come back as a clean structural failure, because deserializeScene promises
// never to throw and never to return partially-populated state -- an abort
// through Scene::fromEntities' debug-only precondition assert would break that
// promise harder than an exception would.
void testMalformedEntityFieldsRejected() {
    const char* cases[] = {
        // id 0 is InvalidEntityId and would violate fromEntities' precondition.
        R"({"version":1,"entities":[{"id":0,"name":"n","assetId":"a","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})",
        R"({"version":1,"entities":[{"id":-3,"name":"n","assetId":"a","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})",
        R"({"version":1,"entities":[{"id":1.5,"name":"n","assetId":"a","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})",
        R"({"version":1,"entities":[{"id":1,"name":5,"assetId":"a","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})",
        R"({"version":1,"entities":[{"id":1,"name":"n","assetId":7,"transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})",
        // transform missing / not an object / short and long vectors / non-numbers.
        R"({"version":1,"entities":[{"id":1,"name":"n","assetId":"a"}]})",
        R"({"version":1,"entities":[{"id":1,"name":"n","assetId":"a","transform":[0,0,0]}]})",
        R"({"version":1,"entities":[{"id":1,"name":"n","assetId":"a","transform":{"position":[0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})",
        R"({"version":1,"entities":[{"id":1,"name":"n","assetId":"a","transform":{"position":[0,0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})",
        R"({"version":1,"entities":[{"id":1,"name":"n","assetId":"a","transform":{"position":["x",0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})",
        R"({"version":1,"entities":[{"id":1,"name":"n","assetId":"a","transform":{"rotation":[0,0,0],"scale":[1,1,1]}}]})",
    };
    const char* labels[] = {"id 0",          "negative id",   "non-integer id",
                            "name not string", "assetId not string", "transform missing",
                            "transform not object", "2-component position",
                            "4-component position", "non-numeric component",
                            "position missing"};
    for (int i = 0; i < 11; ++i) {
        LoadResult r = noto3d::serialization::deserializeScene(cases[i]);
        char name[160];
        std::snprintf(name, sizeof(name), "malformed entity: %s -> nullopt", labels[i]);
        check(!r.scene.has_value(), name);
        std::snprintf(name, sizeof(name), "malformed entity: %s -> diagnostic", labels[i]);
        check(!r.diagnostics.empty(), name);
    }
}

// Q4: the diagnostic must DISTINGUISH an unsupported version from a malformed
// one. Three different user mistakes, so three different messages — a single
// shared string would leave the user guessing which mistake they made.
void testVersionDiagnosticsAreDistinct() {
    const char* missing = R"({"entities":[]})";
    const char* stringy = R"({"version":"1","entities":[]})";
    const char* wrong = R"({"version":2,"entities":[]})";

    const std::string dm = noto3d::serialization::deserializeScene(missing).diagnostics[0];
    const std::string ds = noto3d::serialization::deserializeScene(stringy).diagnostics[0];
    const std::string dw = noto3d::serialization::deserializeScene(wrong).diagnostics[0];

    check(dm.find("missing") != std::string::npos,
          "Q4: missing version diagnostic says 'missing'");
    check(ds.find("malformed") != std::string::npos,
          "Q4: non-integer version diagnostic says 'malformed'");
    check(dw.find("unsupported") != std::string::npos,
          "Q4: wrong-integer version diagnostic says 'unsupported'");
    check(dm != ds && ds != dw && dm != dw,
          "Q4: all three version diagnostics are distinct strings");
}

// Duplicate ids would trip Scene::fromEntities' uniqueness assert, aborting the
// process instead of returning a diagnostic. The serializer must reject them
// itself so the "never throws" contract holds in debug builds too.
void testDuplicateIdsRejected() {
    const char* dup = R"({"version":1,"entities":[
      {"id":1,"name":"a","assetId":"x","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}},
      {"id":1,"name":"b","assetId":"y","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})";
    LoadResult r = noto3d::serialization::deserializeScene(dup);
    check(!r.scene.has_value(), "duplicate ids -> nullopt (no abort in fromEntities)");
    check(!r.diagnostics.empty(), "duplicate ids -> diagnostic");

    // A non-adjacent duplicate must be caught too, not just a consecutive pair.
    const char* far = R"({"version":1,"entities":[
      {"id":7,"name":"a","assetId":"x","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}},
      {"id":8,"name":"b","assetId":"y","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}},
      {"id":7,"name":"c","assetId":"z","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})";
    LoadResult r2 = noto3d::serialization::deserializeScene(far);
    check(!r2.scene.has_value(), "non-adjacent duplicate ids -> nullopt");
    check(!r2.diagnostics.empty(), "non-adjacent duplicate ids -> diagnostic");
}

// Invariant: determinism. The same Scene serializes to byte-identical text.
void testDeterminism() {
    Scene scene;
    const EntityId id1 = scene.createEntity("cube", "noto_cube");
    const EntityId id2 = scene.createEntity("sphere", "noto_sphere");
    Entity& e1 = *scene.findEntity(id1);
    Entity& e2 = *scene.findEntity(id2);
    setTransform(e1, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f);
    setTransform(e2, -1e-7f, 1e7f, 0.0f, 1.0f, 2.0f, 3.0f, 1.0f, 1.0f, 1.0f);

    const std::string a = noto3d::serialization::serializeScene(scene);
    const std::string b = noto3d::serialization::serializeScene(scene);
    check(a == b, "determinism: identical text on repeat serialize");

    // Creation order is preserved in the output, not sorted by id.
    LoadResult r = noto3d::serialization::deserializeScene(a);
    check(r.scene.has_value(), "determinism: output reloads");
    if (!r.scene.has_value()) return;
    if (r.scene->entities().size() != 2) return;
    check(r.scene->entities()[0].id == id1 && r.scene->entities()[1].id == id2,
          "determinism: entities in creation order");
}

// Float-exactness across the full range of magnitudes, using exact equality.
void testFloatExactnessWideRange() {
    const float values[] = {0.0f, -0.0f, 1.0f, -1.0f, 0.1f, -0.1f, 1.0f / 3.0f,
                            1e-7f, 1e-45f, 3.4028235e38f, -3.4028235e38f, 123456.789f};
    for (float v : values) {
        Scene scene;
        const EntityId id = scene.createEntity("n", "a");
        setTransform(*scene.findEntity(id), v, -v, v * 0.5f, v, 0.0f, -v, 1.0f, v, -v);
        LoadResult r =
            noto3d::serialization::deserializeScene(noto3d::serialization::serializeScene(scene));
        if (!r.scene.has_value() || r.scene->entities().empty()) {
            check(false, "float-exactness: load succeeded");
            return;
        }
        const Entity& b = r.scene->entities()[0];
        char name[128];
        std::snprintf(name, sizeof(name), "float-exact: %.9g round-trips bit-identical", v);
        check(b.transform.position.x == v && b.transform.position.y == -v &&
                  b.transform.position.z == v * 0.5f && b.transform.rotation.x == v &&
                  b.transform.rotation.y == 0.0f && b.transform.rotation.z == -v &&
                  b.transform.scale.x == 1.0f && b.transform.scale.y == v &&
                  b.transform.scale.z == -v,
              name);
    }

    // -0.0f is the one value the == comparisons above cannot pin, because
    // -0.0f == 0.0f is true: a serializer that emitted "0.0" for a negative zero
    // would round-trip to the wrong bit pattern and still pass.  Exactness here
    // means the sign survives, so assert it explicitly.
    Scene scene;
    const EntityId id = scene.createEntity("n", "a");
    scene.findEntity(id)->transform.position.x = -0.0f;
    LoadResult r =
        noto3d::serialization::deserializeScene(noto3d::serialization::serializeScene(scene));
    check(r.scene.has_value() && !r.scene->entities().empty() &&
              std::signbit(r.scene->entities()[0].transform.position.x),
          "float-exact: -0.0f keeps its sign bit");
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-serialization-tests");

    testRoundTrip();
    testQ1ReuseThroughSaveLoad();
    testEmptyDocument();
    testUnknownKeysIgnored();
    testVersionGate();
    testVersionDiagnosticsAreDistinct();
    testStructuralFailureInvariants();
    testMalformedEntityFieldsRejected();
    testDuplicateIdsRejected();
    testDeterminism();
    testFloatExactnessWideRange();

    if (g_failures == 0) {
        NOTO3D_INFO("ALL PASS");
        return EXIT_SUCCESS;
    }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
