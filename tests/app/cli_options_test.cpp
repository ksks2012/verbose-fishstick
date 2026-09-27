#include "cli_options.hpp"
#include "log.hpp"

#include <cstdlib>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) { NOTO3D_INFO("PASS  {}", name); }
    else       { NOTO3D_ERROR("FAIL  {}", name); ++g_failures; }
}

using namespace noto3d::app;

// Build a mutable argv for parseOptions. argv[0] is the program name and is
// never parsed, mirroring how main() is invoked.
bool parse(std::vector<std::string> args, Options& out) {
    std::vector<std::string> storage;
    storage.reserve(args.size() + 1);
    storage.emplace_back("noto3d-editor");
    for (auto& s : args) storage.push_back(s);

    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (auto& s : storage) argv.push_back(const_cast<char*>(s.c_str()));
    return parseOptions(static_cast<int>(argv.size()), argv.data(), out);
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-cli-options-tests");

    // ── Test 1: no arguments → defaults (unlimited frames, default scene) ─────
    {
        Options o;
        check(parse({}, o),                       "default: parse succeeds");
        check(o.frameLimit == -1,                 "default: frameLimit == -1 (unlimited)");
        check(!o.assetId.has_value(),              "default: no assetId");
        check(!o.demo,                            "default: demo == false");
        check(!o.listAssets,                      "default: listAssets == false");
    }

    // ── Test 2: --asset=<id> for every renderable fixture id ─────────────────
    {
        for (const char* id : {"smile", "crown", "sunglasses"}) {
            Options o;
            check(parse({std::string("--asset=") + id}, o),
                  "--asset=<id>: parse succeeds");
            check(o.assetId.has_value() && *o.assetId == id,
                  "--asset=<id>: assetId parsed");
            check(!o.demo && o.frameLimit == -1,  "--asset=<id>: other fields default");
        }
    }

    // ── Test 3: --demo → demo-only entry mode ────────────────────────────────
    {
        Options o;
        check(parse({"--demo"}, o),   "--demo: parse succeeds");
        check(o.demo,                 "--demo: demo == true");
        check(!o.assetId.has_value(), "--demo: no assetId");
    }

    // ── Test 4: --list-assets → pre-GL diagnostic mode ───────────────────────
    {
        Options o;
        check(parse({"--list-assets"}, o),    "--list-assets: parse succeeds");
        check(o.listAssets,                   "--list-assets: listAssets == true");
        check(!o.demo && !o.assetId.has_value(),
              "--list-assets: no render mode selected");
    }

    // ── Test 5: validated --frames= (ticket 0011-⑨) ──────────────────────────
    {
        Options o;
        check(parse({"--frames=5"}, o),  "--frames=5: parse succeeds");
        check(o.frameLimit == 5,         "--frames=5: frameLimit == 5");

        Options z;
        check(parse({"--frames=0"}, z),  "--frames=0: parse succeeds");
        check(z.frameLimit == 0,         "--frames=0: frameLimit == 0 (valid edge)");
    }

    // ── Test 6: combined flags keep every field ──────────────────────────────
    {
        Options o;
        check(parse({"--frames=3", "--asset=smile"}, o),
              "combined: --frames + --asset parse succeeds");
        check(o.frameLimit == 3 && o.assetId.has_value() && *o.assetId == "smile",
              "combined: frames and asset both parsed");
    }

    // ── Test 7: unknown asset id is NOT a parse error (registry-layer check) ──
    {
        Options o;
        check(parse({"--asset=nope"}, o),
              "unknown --asset: parse succeeds (validation is registry-layer)");
        check(o.assetId.has_value() && *o.assetId == "nope",
              "unknown --asset: id preserved for the runtime check");
    }

    // ── Test 8: --asset + --demo is rejected by main(), not by parseOptions ───
    {
        Options o;
        check(parse({"--asset=smile", "--demo"}, o),
              "--asset + --demo: parse succeeds (exclusivity checked in main())");
        check(o.assetId.has_value() && o.demo,
              "--asset + --demo: both fields set for the main() check");
    }

    // ── Test 9: unknown flag rejected (0022 hardening) ───────────────────────
    {
        Options o;
        check(!parse({"--no-such-flag"}, o), "unknown flag: rejected");
        check(!parse({"--asset=smile", "--bogus"}, o),
              "unknown flag after a valid flag: rejected");
        check(!parse({"--demo", "--bogus=1"}, o),
              "unknown flag with a value: rejected");
    }

    // ── Test 10: malformed --frames= rejected ────────────────────────────────
    {
        Options o;
        check(!parse({"--frames=abc"}, o), "--frames=abc: rejected");
        check(!parse({"--frames=-1"}, o),  "--frames=-1: rejected");
        check(!parse({"--frames=5x"}, o),  "--frames=5x: rejected (trailing garbage)");
        check(!parse({"--frames="}, o),    "--frames= : rejected");
    }

    // ── Test 11: empty --asset= rejected ─────────────────────────────────────
    {
        Options o;
        check(!parse({"--asset="}, o), "--asset= (empty id): rejected");
    }

    // ── Test 12: repeated --asset keeps the last value (documented) ──────────
    {
        Options o;
        check(parse({"--asset=smile", "--asset=crown"}, o),
              "repeated --asset: parse succeeds");
        check(o.assetId.has_value() && *o.assetId == "crown",
              "repeated --asset: last occurrence wins");
    }

    if (g_failures == 0) { NOTO3D_INFO("ALL PASS"); return EXIT_SUCCESS; }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
