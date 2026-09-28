#include "editor.hpp"
#include "log.hpp"

#include <cstdlib>
#include <string>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

using namespace noto3d::app;
using namespace noto3d::scene;

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-editor-tests");

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

        ed.deselect();
        check(!ed.hasSelection(),               "deselect: hasSelection false");
        check(ed.selectedId() == InvalidEntityId, "deselect: selectedId == Invalid");
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

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
