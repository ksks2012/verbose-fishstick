#include "editor_entry.hpp"

#include <asset_cpu.hpp>
#include <log.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace noto3d::app {
namespace {

// Window title suffix is part of the frozen title contract (ADR-0010; 0022).
constexpr const char* kTitleSuffix = "noto3d (M4)";

const char* geometrySourceName(const asset::AssetEntry& e) {
    using asset::GeometrySource;
    if (!e.geometrySource) return "null";
    return *e.geometrySource == GeometrySource::Placeholder ? "placeholder" : "official";
}

const char* referenceName(const asset::AssetEntry& e) {
    return e.referenceImage ? e.referenceImage->c_str() : "(none)";
}

std::string manifestPath() { return std::string(NOTO3D_ASSETS_DIR) + "/manifest.json"; }

}  // namespace

int runListAssetsCommand() {
    // ADR-0012 ③ — the pre-GL diagnostic resolves its listing through the CPU
    // core, not through AssetRegistry, so this path has no AssetRegistry in it
    // at all and cannot reach the GL facade even in principle.
    //
    // It deliberately drives the SAME loadManifestCpu() pipeline that
    // loadManifestOnly() drives, because the output includes more than the
    // listing: every diagnostic is printed as M3_WARN, and those include
    // per-asset geometry, up_axis and diffuseTextureRef warnings.  Calling
    // parseManifest() alone would print the manifest lines byte-identically but
    // silently drop those warnings — a behaviour change that today's clean
    // manifest happens to hide.  Sharing the pipeline is what makes
    // "identical" structural instead of accidental.
    const std::string manifest = manifestPath();

    const asset::ManifestParse parsed = asset::parseManifest(manifest);
    std::vector<asset::LoadDiagnostic> diagnostics(parsed.diagnostics);
    asset::CpuCache                    cache;
    (void)asset::loadManifestCpu(parsed.entries,
                                 std::filesystem::path(manifest).parent_path(),
                                 cache,
                                 diagnostics);

    for (const auto& d : diagnostics) {
        NOTO3D_WARN("M3_WARN  [{}] {}", d.assetId.empty() ? "manifest" : d.assetId, d.message);
    }
    NOTO3D_INFO("M3_ASSETS version=1");
    for (const auto& e : parsed.entries) {
        NOTO3D_INFO("M3_ASSETS id='{}' geometry={} reference={}",
                    e.id, geometrySourceName(e), referenceName(e));
    }
    NOTO3D_INFO("M3_ASSETS end count={}", static_cast<int>(parsed.entries.size()));
    return EXIT_SUCCESS;
}

std::optional<EntryScene> buildEntryScene(const Options& opts, asset::AssetRegistry& registry) {
    EntryScene entry;

    if (opts.demo) {
        // Demo-only Scene: exactly one entity, identity transform (0014 fixture).
        const renderer::Model* demoPtr = registry.loadModelFile(
            std::string(NOTO3D_ASSETS_DIR) +
                "/geometry/placeholders/demo_checker/demo_checker.obj",
            "demo_checker");
        if (!demoPtr) {
            NOTO3D_CRITICAL("M2_FAIL: demo fixture failed to load");
            return std::nullopt;
        }
        bool textured = false;
        for (const auto& sm : *demoPtr) {
            textured = textured || sm.material.texture.has_value();
        }
        if (textured) {
            NOTO3D_INFO("M2_OK   demo_checker fixture rendered with diffuse "
                        "texture (0009 E2E)");
        } else {
            NOTO3D_WARN("M2_WARN  demo_checker fixture rendered WITHOUT "
                        "a diffuse texture");
        }
        entry.editor.createEntity("demo_checker", "demo_checker");  // identity transform
        entry.sceneName = "demo_checker";
        entry.resolve = [demoPtr](const std::string& id) -> const renderer::Model* {
            return (id == "demo_checker") ? demoPtr : nullptr;
        };
        entry.windowTitle = entry.sceneName + " [placeholder] | " + kTitleSuffix;

    } else if (opts.assetId) {
        // Single-entity Scene from --asset=<id>; M3 identity line kept (0017).
        const asset::AssetEntry* entryPtr = registry.findEntry(*opts.assetId);
        if (!entryPtr) {
            NOTO3D_CRITICAL("M2_FAIL: unknown asset '{}'", *opts.assetId);
            return std::nullopt;
        }
        NOTO3D_INFO("M3_INFO  selected asset '{}' geometry={} reference={}",
                    entryPtr->id, geometrySourceName(*entryPtr), referenceName(*entryPtr));
        entry.editor.createEntity(entryPtr->id, entryPtr->id);  // identity transform
        entry.sceneName = entryPtr->id;
        entry.resolve = [&registry](const std::string& id) -> const renderer::Model* {
            return registry.model(id);
        };
        entry.windowTitle =
            entry.sceneName + " [" + geometrySourceName(*entryPtr) + "] | " + kTitleSuffix;

    } else {
        // Default: m4_vertical_scene — 3 entities spread on the X axis.
        const auto addAt = [&entry](const std::string& assetId, float x) {
            const auto eid = entry.editor.createEntity(assetId, assetId);
            entry.editor.setEntityPosition(eid, {x, 0.0f, 0.0f});
        };
        addAt("smile",     -1.5f);
        addAt("crown",      0.0f);
        addAt("sunglasses", 1.5f);
        entry.resolve = [&registry](const std::string& id) -> const renderer::Model* {
            return registry.model(id);
        };
        entry.sceneName  = "m4_vertical_scene";
        entry.windowTitle = entry.sceneName + " | " + kTitleSuffix;
    }

    // Structured log — 0024's automated matrix asserts on this line.
    NOTO3D_INFO("M4_INFO rendering scene '{}' entities={}",
                entry.sceneName, static_cast<int>(entry.editor.entityCount()));

    return entry;
}

}  // namespace noto3d::app
