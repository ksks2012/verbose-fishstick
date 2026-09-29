#include "editor.hpp"
#include "log.hpp"

#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

using namespace noto3d::app;
using namespace noto3d::scene;

// Log capture seam so the M5_SEL selection line is ASSERTED (ticket 0033 close
// matrix: "selection" must be observable in the structured runtime log).
class CaptureSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    std::vector<std::string> lines;

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        lines.emplace_back(formatted.data(), formatted.size());
    }
    void flush_() override {}
};

bool logHas(const std::vector<std::string>& lines, const std::string& needle) {
    return std::any_of(lines.begin(), lines.end(),
                       [&](const std::string& l) { return l.find(needle) != std::string::npos; });
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-editor-tests");
    auto capture = std::make_shared<CaptureSink>();
    spdlog::default_logger()->sinks().push_back(capture);

    // ── createEntity / entityCount / hierarchyItems ──────────────────────────
    {
        Editor ed;
        check(ed.entityCount() == 0, "initial: entityCount == 0");
        check(ed.hierarchyItems().empty(), "initial: hierarchyItems empty");

        const auto id1 = ed.createEntity("smile", "smile");
        const auto id2 = ed.createEntity("crown", "crown");
        const auto id3 = ed.createEntity("sunglasses", "sunglasses");

        check(ed.entityCount() == 3, "after 3 creates: count == 3");

        const auto items = ed.hierarchyItems();
        check(items.size() == 3, "hierarchyItems: size == 3");
        // Creation order preserved
        check(items[0].id == id1 && items[0].name == "smile",     "order: [0] is smile");
        check(items[1].id == id2 && items[1].name == "crown",     "order: [1] is crown");
        check(items[2].id == id3 && items[2].name == "sunglasses","order: [2] is sunglasses");
        check(items[0].assetId == "smile",     "assetId: smile");
        check(items[2].assetId == "sunglasses","assetId: sunglasses");
    }

    // ── destroyEntity ────────────────────────────────────────────────────────
    {
        Editor ed;
        const auto id1 = ed.createEntity("a", "x");
        const auto id2 = ed.createEntity("b", "y");
        const auto id3 = ed.createEntity("c", "z");

        check(ed.destroyEntity(id2), "destroyEntity: returns true for known id");
        check(ed.entityCount() == 2, "after destroy: count == 2");
        const auto items = ed.hierarchyItems();
        check(items[0].id == id1 && items[1].id == id3,
              "after destroy middle: order is [a, c]");

        check(!ed.destroyEntity(id2), "destroyEntity: returns false for unknown id");
    }

    // ── clear ────────────────────────────────────────────────────────────────
    {
        Editor ed;
        ed.createEntity("x", "x");
        ed.createEntity("y", "y");
        ed.clear();
        check(ed.entityCount() == 0, "clear: entityCount == 0");
        check(ed.hierarchyItems().empty(), "clear: hierarchyItems empty");
    }

    // ── setEntityPosition ────────────────────────────────────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("cube", "cube");
        check(ed.setEntityPosition(id, {1.0f, 2.0f, 3.0f}),
              "setEntityPosition: returns true for known id");
        // Verify through scene() read-only seam
        const auto* e = ed.scene().findEntity(id);
        check(e && e->transform.position.x == 1.0f &&
                   e->transform.position.y == 2.0f &&
                   e->transform.position.z == 3.0f,
              "setEntityPosition: position visible through scene()");
        check(!ed.setEntityPosition(InvalidEntityId, {0,0,0}),
              "setEntityPosition: false for invalid id");
    }

    // ── setEntityAssetId — thin assetId mutation, limited to that field ─────
    {
        Editor ed;
        const auto id1 = ed.createEntity("smile", "smile");
        const auto id2 = ed.createEntity("crown", "crown");
        ed.setEntityPosition(id1, {1.0f, 2.0f, 3.0f});

        check(ed.setEntityAssetId(id1, "warm"),
              "setEntityAssetId: returns true for known id");
        {
            const auto items = ed.hierarchyItems();
            check(items.size() == 2, "setEntityAssetId: entity count unchanged");
            check(items[0].assetId == "warm",     "setEntityAssetId: assetId updated");
            check(items[1].assetId == "crown",    "setEntityAssetId: other entity unaffected");
        }
        check(ed.setEntityAssetId(id2, "cool"),
              "setEntityAssetId: second id also true");
        check(!ed.setEntityAssetId(InvalidEntityId, "warm"),
              "setEntityAssetId: false for invalid id");
    }

    // ── selection basics ─────────────────────────────────────────────────────
    {
        Editor ed;
        check(!ed.hasSelection(),               "initial: no selection");
        check(ed.selectedId() == InvalidEntityId, "initial: selectedId == Invalid");
        check(!ed.selectedName(),               "initial: selectedName empty");
        check(!ed.selectedAssetId(),            "initial: selectedAssetId empty");

        const auto id = ed.createEntity("smile", "smile");
        ed.select(id);
        check(ed.selectedId() == id,            "select: selectedId correct");
        check(ed.hasSelection(),                "select: hasSelection true");
        check(ed.selectedName()    && *ed.selectedName()    == "smile",  "selectedName");
        check(ed.selectedAssetId() && *ed.selectedAssetId() == "smile", "selectedAssetId");
        check(logHas(capture->lines,
                     "M5_SEL  entity='smile' id=" +
                         std::to_string(static_cast<unsigned>(id))),
              "select: logged M5_SEL with entity name and id");

        ed.deselect();
        check(!ed.hasSelection(),               "deselect: hasSelection false");
        check(ed.selectedId() == InvalidEntityId, "deselect: selectedId == Invalid");

        // selection now covered by M5_SEL assertion (above); add a count check
        // that deselect does NOT log a second M5_SEL line (silent deselect).
        check(std::count_if(capture->lines.begin(), capture->lines.end(),
                            [](const std::string& l) {
                                return l.find("M5_SEL  entity=") != std::string::npos;
                            }) == 1,
              "select: exactly one M5_SEL line (deselect is silent)");
    }

    // ── selection survives frames (persistence) ──────────────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("crown", "crown");
        ed.select(id);

        // Simulate multiple frames: selection still valid
        for (int frame = 0; frame < 5; ++frame) {
            check(ed.hasSelection(),        "selection survives frame");
            check(ed.selectedId() == id,    "selectedId consistent across frames");
        }
    }

    // ── destroyEntity clears selection when selected ─────────────────────────
    {
        Editor ed;
        const auto id1 = ed.createEntity("a", "a");
        const auto id2 = ed.createEntity("b", "b");
        ed.select(id1);
        check(ed.hasSelection(), "before destroy: selected");

        ed.destroyEntity(id1);  // destroy the selected entity
        check(!ed.hasSelection(),               "after destroy selected: no selection");
        check(ed.selectedId() == InvalidEntityId, "after destroy: selectedId == Invalid");

        // Remaining entity unaffected
        check(ed.entityCount() == 1, "after destroy: count == 1");
        check(ed.hierarchyItems()[0].id == id2, "after destroy: remaining entity correct");
    }

    // ── destroyEntity of non-selected leaves selection intact ─────────────────
    {
        Editor ed;
        const auto id1 = ed.createEntity("a", "a");
        const auto id2 = ed.createEntity("b", "b");
        ed.select(id1);
        ed.destroyEntity(id2);  // destroy the OTHER entity
        check(ed.hasSelection(),     "non-selected destroy: selection intact");
        check(ed.selectedId() == id1,"non-selected destroy: selectedId unchanged");
    }

    // ── clear() also clears selection ────────────────────────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("x", "x");
        ed.select(id);
        ed.clear();
        check(!ed.hasSelection(), "clear: selection cleared");
        check(ed.entityCount() == 0, "clear: entities removed");
    }

    // ── select stale id: stored but hasSelection() false ─────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("x", "x");
        ed.destroyEntity(id);  // entity gone
        ed.select(id);          // try to select the stale id
        check(ed.selectedId() == id, "stale select: selectedId stored");
        check(!ed.hasSelection(),    "stale select: hasSelection false (entity gone)");
        check(!ed.selectedName(),    "stale select: selectedName empty");
    }

    // ── scene() read-only access from rendering seam ─────────────────────────
    {
        Editor ed;
        ed.createEntity("a", "a");
        ed.createEntity("b", "b");
        const auto& s = ed.scene();
        check(s.entities().size() == 2, "scene(): entity count visible");
    }

    // ── facade-level id monotonicity across a destroy/create cycle ───────────
    // M6 identity contract (ADR-0010 Q2/Q4): ids are monotonic, never reused,
    // and emptying the Scene does NOT reset the counter (ticket 0032).
    {
        Editor ed;
        const auto id1 = ed.createEntity("a", "a");
        const auto id2 = ed.createEntity("b", "b");
        ed.destroyEntity(id1);
        ed.destroyEntity(id2);
        check(ed.entityCount() == 0, "cycle: all destroyed → count 0");

        const auto id3 = ed.createEntity("c", "c");
        check(id3 > id2, "cycle: new id is greater than any prior id (never reused)");
        check(ed.entityCount() == 1, "cycle: count back to one");

        ed.clear();
        check(ed.entityCount() == 0, "clear: emptied again");
        const auto id4 = ed.createEntity("d", "d");
        check(id4 > id3, "id: counter advances past id3 after clear (never reset)");
    }

    // ── facade-level no-op destroys ──────────────────────────────────────────
    {
        Editor ed;
        const auto idA = ed.createEntity("a", "a");
        check(!ed.destroyEntity(InvalidEntityId), "no-op: invalid id returns false");
        check(!ed.destroyEntity(idA + 99),         "no-op: unknown id returns false");
        check(ed.destroyEntity(idA),               "no-op: real id returns true");
        check(!ed.destroyEntity(idA),              "no-op: already-destroyed id returns false");
        check(ed.entityCount() == 0, "no-op: no entities created or resurrected");
    }

    // ── replaceScene: the single load injection point (ADR-0013 Q6) ──────────
    {
        // The M5 type boundary is a *type* property, so pin it with
        // static_assert rather than prose.  Note the second assert: a plain
        // `is_callable`-style check would also pass for a `Scene&` parameter,
        // which is exactly the spelling ADR-0011 forbids.  Taking the address of
        // the member and comparing the whole signature pins the parameter type
        // itself, so by-value stops being a claim about the header.
        static_assert(
            std::is_same_v<decltype(std::declval<const Editor&>().scene()),
                           const Scene&>,
            "Editor::scene() must keep returning const Scene& — M5 type boundary");
        static_assert(
            std::is_same_v<decltype(&Editor::replaceScene),
                           void (Editor::*)(Scene) noexcept>,
            "replaceScene must take Scene BY VALUE, return void and be noexcept — a "
            "Scene& or Scene&& parameter would breach the M5 type boundary.  "
            "noexcept is part of the type since C++17, so this pins that too.");

        Editor ed;
        const auto original = ed.createEntity("original", "smile");
        ed.select(original);
        check(ed.hasSelection(), "replaceScene: a selection exists beforehand");

        // A restored Scene built in memory — no file I/O in this ticket.
        Entity r1; r1.id = 41; r1.name = "restored-one"; r1.assetId = "crown";
        r1.transform.position = glm::vec3(1.0f, 0.0f, 0.0f);
        Entity r2; r2.id = 12; r2.name = "restored-two"; r2.assetId = "sun";
        auto restored = Scene::fromEntities({r1, r2});

        ed.replaceScene(std::move(restored));

        check(ed.entityCount() == 2, "replaceScene: the supplied entities are in place");
        const auto items = ed.hierarchyItems();
        check(items.size() == 2 && items[0].id == 41 && items[0].name == "restored-one" &&
                  items[0].assetId == "crown" && items[1].id == 12 &&
                  items[1].name == "restored-two" && items[1].assetId == "sun",
              "replaceScene: the entities are exactly the ones supplied, in order");
        check(ed.entityName(original) == std::nullopt,
              "replaceScene: the previous scene's entities are gone");

        // Selection is editor-session state, not persisted data (Q2/Q6).
        check(!ed.hasSelection(), "replaceScene: selection is cleared");
        check(ed.selectedId() == InvalidEntityId,
              "replaceScene: selectedId is InvalidEntityId after the swap");

        // The derived allocator travelled with the scene.
        const auto nextId = ed.createEntity("added", "warm");
        check(nextId == 42, "replaceScene: the derived allocator survives the swap");
    }

    // ── replaceScene: a selection that WOULD still resolve must still die ────
    //
    // The block above cannot distinguish a real deselect() from no
    // deselect() at all: `hasSelection()`/`selectedName()` both go through
    // findEntity(selection_), and the previously selected id no longer exists,
    // so they report "nothing selected" either way.  The dangerous case is the
    // opposite one — a restored scene that HAPPENS TO contain the same id — so
    // that is what this block pins.
    {
        Editor ed;
        const auto id = ed.createEntity("local", "smile");
        ed.select(id);
        check(ed.selectedName().value_or("") == "local",
              "collide: the id is selected and resolvable before the swap");

        Entity sameId;
        sameId.id      = id;  // the restored scene reuses the selected id
        sameId.name    = "restored-but-different";
        sameId.assetId = "crown";
        ed.replaceScene(Scene::fromEntities({sameId}));

        check(ed.entityName(id).value_or("") == "restored-but-different",
              "collide: the same id DOES exist in the new scene, under a new name");
        check(ed.selectedId() == InvalidEntityId,
              "collide: a resolvable id is still not selected after replaceScene");
        check(!ed.hasSelection(),
              "collide: hasSelection() is false even though the id resolves");
        check(ed.selectedName() == std::nullopt,
              "collide: selectedName() is nullopt, not the restored entity's name");
    }

    // ── replaceScene: a stale selection cannot survive ───────────────────────
    {
        Editor ed;
        const auto id = ed.createEntity("a", "a");
        ed.select(id);
        ed.replaceScene(Scene::fromEntities({}));
        check(!ed.hasSelection(), "replaceScene: selecting then replacing an empty "
                                  "scene leaves nothing selected");
        check(ed.selectedId() == InvalidEntityId,
              "replaceScene: no dangling selection after replacing with an empty set");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
