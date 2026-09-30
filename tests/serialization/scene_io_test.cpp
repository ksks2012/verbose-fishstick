#include "scene_io.hpp"
#include "log.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
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


// ===========================================================================
// Ticket 0044 -- strict structure, lenient references.
//
// ONE table drives every structural assertion, and 0043's four ad-hoc case
// loops were migrated into it.  That is deliberate: this repo rejects duplicate
// policy on the grounds that two copies "can drift" (ADR-0012 (3), ADR-0013
// Q11), and a defect listed in two tables is a defect half the suite forgets to
// check.  Adding a structural defect is now one row, in one place.
struct StructuralCase {
    const char* label;         // what a user did wrong
    std::string document;      // the offending document
    const char* mustMention;    // the defect must be NAMED
    const char* statesExpectation;  // the fix must be stated, or ""
    const char* mustNotMention; // the NEIGHBOUR guard's wording, or ""
};

// The transform appears in nearly every case; spelling it once means a change to
// the v1 document shape is one edit here rather than one per row.
constexpr std::string_view kTransform =
    R"({"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]})";

std::string rawEntity(std::string_view fields) {
    return "{" + std::string(fields) + "}";
}

std::string withTransform(std::string_view fields,
                          std::string_view transform = kTransform) {
    return rawEntity(std::string(fields) + R"(,"transform":)" + std::string(transform));
}

// A valid entity with the given fields plus a well-formed transform.  Cases
// that omit or corrupt ONE field use this so every other field is still valid
// and the guard under test is the only thing that can reject the document.
std::string oneEntity(std::string_view id, std::string_view name, std::string_view assetId,
                      std::string_view transform = kTransform) {
    std::string fields;
    fields += R"("id":)" + std::string(id);
    fields += R"(,"name":)" + std::string(name);
    fields += R"(,"assetId":)" + std::string(assetId);
    return withTransform(fields, transform);
}

std::string sceneOf(const std::vector<std::string>& entities) {
    std::string out = R"({"version":1,"entities":[)";
    for (std::size_t i = 0; i < entities.size(); ++i) {
        if (i) out += ",";
        out += entities[i];
    }
    return out + "]}";
}

std::vector<StructuralCase> structuralCases() {
    // NOTE these are raw JSON fragments, not C++ strings to quote: name and
    // assetId need their quotes, an id must stay a JSON NUMBER so it reaches
    // the integer guards (a quoted "1" is the wrong-TYPE case, its own row).
    const std::string n = R"("n")", a = R"("a")", one = "1";
    return {
        // -- JSON shape ----------------------------------------------------
        {"malformed JSON", "not json at all", "JSON", "", ""},
        {"non-object root", "42", "root", "JSON object", ""},
        {"root is a JSON string", R"("just a string")", "root", "JSON object", "version"},
        {"missing version", R"({"entities":[]})", "missing", "integer", ""},
        {"non-integer version", R"({"version":"1","entities":[]})", "malformed", "integer", ""},
        {"unsupported version", R"({"version":2,"entities":[]})", "unsupported", "1", ""},
        // The three version mistakes must not collapse into one message (Q4).
        {"missing entities", R"({"version":1})", "missing", "", "must be an array"},
        {"entities not an array", R"({"version":1,"entities":{}})", "entities", "array", ""},
        // -- entity shape --------------------------------------------------
        {"entity is a number", R"({"version":1,"entities":[5]})", "must be an object", "",
         "missing 'id'"},
        {"entity missing id", sceneOf({rawEntity(R"("name":"n","assetId":"a")")}), "id", "", ""},
        {"id not an integer", sceneOf({oneEntity(R"("1")", n, a)}), "id", "integer", ""},
        {"id 0 is the reserved InvalidEntityId", sceneOf({oneEntity("0", n, a)}), "id",
         "InvalidEntityId", ""},
        {"fractional id", sceneOf({oneEntity("1.5", n, a)}), "id", "integer", ""},
        {"negative id reports the value", sceneOf({oneEntity("-3", n, a)}), "id", "-3", ""},
        // 2^32 narrows to 0 == InvalidEntityId on an unchecked cast: in release it
        // would load with the one id the format forbids, in debug it aborts.
        {"id above EntityId range", sceneOf({oneEntity("4294967296", n, a)}), "id",
         "4294967295", ""},
        {"id one above EntityId range", sceneOf({oneEntity("4294967297", n, a)}), "id",
         "at most", ""},
        {"duplicate id, adjacent",
         sceneOf({oneEntity("4", n, a), oneEntity("4", n, a)}), "entities[1]", "4", ""},
        {"duplicate id, non-adjacent",
         sceneOf({oneEntity("7", n, a), oneEntity("8", n, a), oneEntity("7", n, a)}),
         "entities[2]", "7", ""},
        // -- string fields -------------------------------------------------
        {"name not a string", sceneOf({oneEntity(one, "5", a)}), "name", "string", ""},
        {"name missing", sceneOf({withTransform(R"("id":1,"assetId":"a")")}), "name", "", ""},
        {"assetId not a string", sceneOf({oneEntity(one, n, "7")}), "assetId", "string", ""},
        {"assetId is an array", sceneOf({oneEntity(one, n, R"(["a"])")}), "assetId",
         "string", ""},
        {"assetId missing", sceneOf({withTransform(R"("id":1,"name":"n")")}), "assetId", "", ""},
        // -- transform shape -----------------------------------------------
        {"transform missing", sceneOf({rawEntity(R"("id":1,"name":"n","assetId":"a")")}),
         "transform", "", ""},
        // An array of exactly 3 numbers would PASS the vector checks, so this
        // is the only guard that can catch it.
        {"transform not an object",
         sceneOf({oneEntity(one, n, a, "[0,0,0]")}), "transform", "object", "3 components"},
        {"2-component position",
         sceneOf({oneEntity(one, n, a,
                            R"({"position":[0,0],"rotation":[0,0,0],"scale":[1,1,1]})")}),
         "position", "exactly 3 components", ""},
        {"4-component position",
         sceneOf({oneEntity(one, n, a,
                            R"({"position":[0,0,0,0],"rotation":[0,0,0],"scale":[1,1,1]})")}),
         "position", "exactly 3 components", ""},
        {"non-numeric component",
         sceneOf({oneEntity(one, n, a,
                            R"({"position":["x",0,0],"rotation":[0,0,0],"scale":[1,1,1]})")}),
         "position", "number", ""},
        // The index must survive the nested transform path, so the failing entity
        // is findable in a long document without counting by hand.
        {"bad transform in the 2nd entity names its index",
         sceneOf({oneEntity("1", n, a), oneEntity("2", n, a, R"({"position":[0,0]})")}),
         "entities[1]", "", ""},
        {"position missing",
         sceneOf({oneEntity(one, n, a,
                            R"({"rotation":[0,0,0],"scale":[1,1,1]})")}),
         "position", "missing", ""},
    };
}

// One pass over the table: every case must fail to load, must name its own
// defect, must state the fix, must not borrow a neighbour's wording, and must
// not leak raw parser text as the whole message (Q3).
void testStructuralCasesRejected() {
    const std::vector<StructuralCase> cases = structuralCases();
    std::vector<std::string> seenVersion;
    for (const auto& c : cases) {
        const LoadResult r = noto3d::serialization::deserializeScene(c.document);
        const std::string diag =
            r.diagnostics.empty() ? std::string("<none>") : r.diagnostics.front();
        char name[160];

        std::snprintf(name, sizeof(name), "Q3 strict: %s -> no scene", c.label);
        check(!r.scene.has_value(), name);

        std::snprintf(name, sizeof(name), "Q3 message quality: %s -> has diagnostic", c.label);
        check(!r.diagnostics.empty(), name);

        std::snprintf(name, sizeof(name), "Q3 message quality: %s names the defect", c.label);
        check(diag.find(c.mustMention) != std::string::npos, name);
        if (diag.find(c.mustMention) == std::string::npos) NOTO3D_ERROR("       got: {}", diag);

        if (*c.statesExpectation) {
            std::snprintf(name, sizeof(name), "Q3 expectation stated: %s", c.label);
            check(diag.find(c.statesExpectation) != std::string::npos, name);
            if (diag.find(c.statesExpectation) == std::string::npos)
                NOTO3D_ERROR("       got: {}", diag);
        }
        // A redundant guard would otherwise fire first and produce an equally
        // valid-looking message, so assert the neighbour's wording is ABSENT.
        if (*c.mustNotMention) {
            std::snprintf(name, sizeof(name), "Q3 guard isolation: %s is not its neighbour's",
                          c.label);
            check(diag.find(c.mustNotMention) == std::string::npos, name);
            if (diag.find(c.mustNotMention) != std::string::npos)
                NOTO3D_ERROR("       got: {}", diag);
        }

        std::snprintf(name, sizeof(name), "Q3: %s is not raw parser text", c.label);
        check(!diag.empty() && diag.rfind("scene_io: ", 0) == 0, name);
        if (diag.find("version") != std::string::npos) seenVersion.push_back(diag);
    }
    // Q4: three different version mistakes, three different messages.
    check(seenVersion.size() == 3 && seenVersion[0] != seenVersion[1] &&
              seenVersion[1] != seenVersion[2] && seenVersion[0] != seenVersion[2],
          "Q4: the three version diagnostics are distinct strings");
}

// An entity index in the message is what makes a large document fixable.
void testDiagnosticIdentifiesFailingEntity() {
    // Three good entities, then a broken fourth (missing assetId).
    const std::string doc =
        sceneOf({oneEntity("1", R"("a")", R"("x")"), oneEntity("2", R"("b")", R"("y")"),
                 oneEntity("3", R"("c")", R"("z")"),
                 withTransform(R"("id":4,"name":"d")")});
    LoadResult r = noto3d::serialization::deserializeScene(doc);
    check(!r.scene.has_value(), "Q3: bad 4th entity -> no scene");
    check(!r.diagnostics.empty(), "Q3: bad 4th entity -> diagnostic");
    if (r.diagnostics.empty()) return;
    const std::string& diag = r.diagnostics.front();
    check(diag.find("entities[3]") != std::string::npos,
          "Q3: diagnostic identifies the failing entity index (entities[3])");
    if (diag.find("entities[3]") == std::string::npos) NOTO3D_ERROR("       got: {}", diag);
    check(diag.find("assetId") != std::string::npos,
          "Q3: diagnostic identifies the missing field (assetId)");
}

// Lenient references: an assetId the serializer cannot resolve is NOT a defect.
// The 0041 guard makes it structurally impossible for the serializer to consult
// the registry, so it must retain the entity verbatim and let the M4 resolution
// seam skip it at draw time.
void testUnresolvableAssetIdIsRetained() {
    const std::string doc =
        sceneOf({oneEntity("1", R"("cube")", R"("noto_cube")",
                            R"({"position":[1,2,3],"rotation":[0,0,0],"scale":[1,1,1]})"),
                 oneEntity("2", R"("ghost")", R"("asset_that_does_not_exist")",
                            R"({"position":[4,5,6],"rotation":[0,0,0],"scale":[2,2,2]})")});
    LoadResult r = noto3d::serialization::deserializeScene(doc);
    check(r.scene.has_value(), "Q3 lenient: unknown assetId still loads");
    check(r.diagnostics.empty(), "Q3 lenient: unknown assetId yields no diagnostic");
    if (!r.scene.has_value() || r.scene->entities().size() != 2) {
        check(false, "Q3 lenient: both entities retained");
        return;
    }
    const auto& kept = r.scene->entities()[1];
    check(kept.id == 2 && kept.name == "ghost", "Q3 lenient: entity kept with its id and name");
    check(kept.assetId == "asset_that_does_not_exist",
          "Q3 lenient: unresolvable assetId retained VERBATIM");
    check(kept.transform.position == glm::vec3(4.0f, 5.0f, 6.0f),
          "Q3 lenient: retained entity keeps its transform");
}

// An EMPTY assetId is legal input, not a structural defect: present-and-a-string
// is the only rule, so the serializer never judges resolvability -- and
// therefore never judges emptiness either.
void testEmptyAssetIdIsLegal() {
    const std::string doc = sceneOf({oneEntity("1", R"("nameless")", R"("")")});
    LoadResult r = noto3d::serialization::deserializeScene(doc);
    check(r.scene.has_value(), "Q3 lenient: empty assetId loads successfully");
    check(r.diagnostics.empty(), "Q3 lenient: empty assetId yields no diagnostic");
    if (!r.scene.has_value() || r.scene->entities().empty()) {
        check(false, "Q3 lenient: entity with empty assetId retained");
        return;
    }
    check(r.scene->entities()[0].assetId.empty(),
          "Q3 lenient: empty assetId retained as an empty string");
}

// An entity whose name is empty is equally legal -- the serializer validates
// shape, never content it has no opinion about.
void testEmptyNameIsLegal() {
    const std::string doc = sceneOf({oneEntity("1", R"("")", R"("a")")});
    LoadResult r = noto3d::serialization::deserializeScene(doc);
    check(r.scene.has_value() && r.diagnostics.empty(),
          "Q3 lenient: empty name loads without a diagnostic");
}

// The unresolvable entity must SURVIVE a full round-trip too, not merely the
// load: save must not drop or "repair" a reference it cannot resolve.
void testUnresolvableReferenceSurvivesRoundTrip() {
    Scene scene;
    const EntityId id = scene.createEntity("ghost", "asset_that_does_not_exist");
    scene.findEntity(id)->transform.position = glm::vec3(7.0f, 8.0f, 9.0f);
    const std::string text = noto3d::serialization::serializeScene(scene);
    check(text.find("asset_that_does_not_exist") != std::string::npos,
          "Q3 lenient: unresolvable assetId is written out, not dropped");
    LoadResult r = noto3d::serialization::deserializeScene(text);
    check(r.scene.has_value() && !r.scene->entities().empty() &&
              r.scene->entities()[0].assetId == "asset_that_does_not_exist",
          "Q3 lenient: unresolvable assetId survives the full round-trip");
}

// No half-loaded scene, whatever the defect: a document whose LAST entity is
// broken must return nothing at all, not the valid prefix.
void testNoHalfLoadedScene() {
    const std::string doc =
        sceneOf({oneEntity("1", R"("a")", R"("x")"), oneEntity("2", R"("b")", R"("y")"),
                 rawEntity(R"("id":3,"name":"c")")});
    LoadResult r = noto3d::serialization::deserializeScene(doc);
    check(!r.scene.has_value(), "Q3: broken trailing entity -> no partial scene");
    check(!r.diagnostics.empty(), "Q3: broken trailing entity -> diagnostic");
}

#undef E

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-serialization-tests");

    testRoundTrip();
    testQ1ReuseThroughSaveLoad();
    testEmptyDocument();
    testUnknownKeysIgnored();
    testVersionGate();
    testDeterminism();
    testFloatExactnessWideRange();

    testStructuralCasesRejected();
    testDiagnosticIdentifiesFailingEntity();
    testUnresolvableAssetIdIsRetained();
    testEmptyAssetIdIsLegal();
    testEmptyNameIsLegal();
    testUnresolvableReferenceSurvivesRoundTrip();
    testNoHalfLoadedScene();

    if (g_failures == 0) {
        NOTO3D_INFO("ALL PASS");
        return EXIT_SUCCESS;
    }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
