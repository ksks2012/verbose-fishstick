#pragma once

#include <asset_registry.hpp>
#include <cli_options.hpp>
#include <draw_list.hpp>
#include <editor.hpp>

#include <optional>
#include <string>

namespace noto3d::app {

// The Scene for one CLI entry mode, with the resolver and the window title that
// belong to it (ADR-0010 Q6; ticket 0022).
//
// Layering note: this TU needs noto3d_asset, so it is compiled into the editor
// executable rather than into noto3d_app.  Promoting it into the app library is
// a target-graph change, which tickets 0027 / 0031 own — not this file.
struct EntryScene {
    app::Editor  editor;     // owns the Scene by value (ADR-0011 Q11)
    Resolver     resolve;
    // Scene identity, as logged by the M4_INFO line that 0024's matrix asserts
    // on.  Distinct from windowTitle, which is decorated with the asset's
    // geometry source and the editor suffix.
    std::string sceneName;
    std::string windowTitle;
};

// --list-assets diagnostic CLI (ticket 0017).
//
// Loads the manifest to CPU only (no GL context), prints id / geometry source /
// reference per asset, and returns the process exit code.  Not a new
// asset-pipeline capability: it is the existing loadManifestOnly() + entries()
// path used as a pre-GL diagnostic.  The M3_ASSETS log lines are part of the
// 0024 regression matrix.
[[nodiscard]] int runListAssetsCommand();

// Builds the Scene for the selected entry mode: --demo, --asset=<id>, or the
// default vertical scene.
//
// Returns nullopt after logging a CRITICAL diagnostic on failure; the caller
// skips the render loop in that case.  The M3_INFO / M4_INFO lines emitted here
// are asserted by 0024's regression matrix, so they are contract, not chatter.
//
// Every render path still goes through collectDrawItems() — there is no direct
// submit bypass (ticket 0022).
// `preloadedScene` is the --scene= document already deserialized PRE-GL by
// main() (ticket 0046).  It is passed separately from Options because Options is
// the pure-syntax contract of ticket 0025.
//
// It is an optional because an empty Scene is NOT the absent case: a valid
// document with "entities":[] is a real preloaded Scene.  The parameter is
// REQUIRED in the --scene= branch and is intentionally NOT defaulted — that
// prevents the fallback path that silently made the "preloaded" parameter dead
// during the 0046 diff review (see standards review findings).
[[nodiscard]] std::optional<EntryScene> buildEntryScene(
    const Options& opts, asset::AssetRegistry& registry,
    std::optional<scene::Scene> preloadedScene);

}  // namespace noto3d::app
