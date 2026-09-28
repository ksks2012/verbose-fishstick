#include "entity_edit.hpp"
#include "log.hpp"

#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

using namespace noto3d::app;
using namespace noto3d::scene;
using noto3d::asset::GeometrySource;

// Log capture seam so the M5_ENTITY lines are ASSERTED, not just printed
// (an acceptance box of ticket 0032 claims the create/destroy log format).
// Attach to the default logger, run the block under test, then mirror the
// log through to the real sink and assert on the captured text.
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
    noto3d::core::log_init("noto3d-entity-edit-tests");
    std::shared_ptr<CaptureSink> capture = std::make_shared<CaptureSink>();
    // Appended SINK, not a replacement: the normal stdout sink stays (visible
    // PASS/FAIL output + the M5_ENTITY lines), and the captured copy gets
    // asserted below.  `log_init` set the default logger; push the capture.
    spdlog::default_logger()->sinks().push_back(capture);

    // ── commitCreateEntity: creates through the facade and logs ─────────────
    {
        Editor ed;
        check(ed.entityCount() == 0, "create: initial count 0");

        const auto id = commitCreateEntity(ed, AssetChoice{"smile", GeometrySource::Placeholder});
        check(id != InvalidEntityId, "create: returns a valid new id");
        check(ed.entityCount() == 1, "create: count increases by one");

        const auto items = ed.hierarchyItems();
        check(items.size() == 1 && items[0].id == id, "create: row in hierarchy");
        check(items[0].name == "smile",   "create: name defaults to the asset id");
        check(items[0].assetId == "smile","create: assetId carries the picker choice");
        check(logHas(capture->lines,
                     "M5_ENTITY created entity='smile' id=" +
                         std::to_string(static_cast<unsigned>(id)) +
                         " assetId='smile' count=1"),
              "create: logged M5_ENTITY created with new id, assetId, count");
    }

    // ── commitCreateEntity: asset mirrors a picker choice (drawable by
    //    construction — the value comes from the picker's renderable-only list).
    //    Signature takes an AssetChoice (not a bare string), so the app layer
    //    cannot be handed an arbitrary/unresolvable asset id (ticket 0032).
    // ── Creating does NOT auto-select (create is not a selection op) ────────
    {
        Editor ed;
        const auto existing = ed.createEntity("existing", "crown");
        ed.select(existing);  // an existing selection is left untouched by create
        const auto id = commitCreateEntity(ed, AssetChoice{"sun", GeometrySource::Official});
        check(id != InvalidEntityId && ed.entityCount() == 2, "create: added second entity");
        check(ed.selectedId() == existing && ed.hasSelection(), "create: selection untouched");
        check(ed.hierarchyItems()[1].assetId == "sun", "create: new entity uses its choice");
    }

    // ── commitDestroySelected: destroys the selection and logs ───────────────
    {
        Editor ed;
        const auto idA = commitCreateEntity(ed, AssetChoice{"a", GeometrySource::Placeholder});
        (void)commitCreateEntity(ed, AssetChoice{"b", GeometrySource::Official});
        check(ed.entityCount() == 2, "destroy: two entities created");
        check(!ed.hasSelection(), "destroy: nothing selected yet");

        commitDestroySelected(ed);  // nothing selected → silent no-op
        check(ed.entityCount() == 2, "destroy: no-op when nothing selected (no crash)");

        ed.select(idA);
        commitDestroySelected(ed);
        check(ed.entityCount() == 1, "destroy: count decreases by one");
        check(!ed.hasSelection(), "destroy: selection cleared after destroying it");
        check(ed.selectedId() == InvalidEntityId, "destroy: no dangling selection id");
        check(logHas(capture->lines,
                     "M5_ENTITY destroyed entity='a' id=" +
                         std::to_string(static_cast<unsigned>(idA)) + " count=1"),
              "destroy: logged M5_ENTITY destroyed with the destroyed id, count");
        {
            const auto items = ed.hierarchyItems();
            check(items.size() == 1 && items[0].name == "b",
                  "destroy: removed entity gone from hierarchy, order intact");
        }

        commitDestroySelected(ed);  // stale selection id is not a selection
        check(ed.entityCount() == 1,
              "destroy: destroying with stale/absent selection is a safe no-op");
    }

    // ── logs: exactly the operations that happened; no-ops log nothing ──────
    // count the emitted M5_ENTITY lines so a silent no-op is provably silent
    // (the ticket says no-op destroy is not an error — and must not log one).
    {
        std::size_t created = 0, destroyed = 0;
        for (const auto& line : capture->lines) {
            if (line.find("M5_ENTITY created entity=") != std::string::npos) { ++created; }
            if (line.find("M5_ENTITY destroyed entity=") != std::string::npos) { ++destroyed; }
        }
        check(created == 4,  "logs: exactly 4 M5_ENTITY created lines (smile, sun, a, b)");
        check(destroyed == 1, "logs: exactly 1 M5_ENTITY destroyed line (only the real destroy)");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}