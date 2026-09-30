#pragma once

#include <optional>
#include <string>

namespace noto3d::app {

// Parsed command-line options for the editor.
//
// Layering (ticket 0025): this is a *pure syntax* contract. It validates option
// shape only — mutual exclusion of entry modes, unknown asset ids and every
// runtime concern stay in main()/the runtime, so the CLI contract is testable
// headlessly without a GL context or a subprocess.
struct Options {
    int                        frameLimit = -1;   // < 0 = unlimited
    std::optional<std::string> assetId;           // --asset=<id> (manifest asset)
    bool                       demo       = false; // --demo (demo_checker fixture)
    bool                       listAssets = false; // --list-assets (pre-GL diagnostic)
    // ADR-0013 Q5: --scene= is the THIRD mutually exclusive entry mode.  The
    // exclusion is still not decided here — main() owns it, pre-GL, exactly as
    // it owns --asset + --demo (ticket 0025's pure-syntax rule).
    std::optional<std::string> scenePath;         // --scene=<path>
    // ADR-0013 Q7: an OUTPUT option, deliberately NOT part of the entry-mode
    // set, so it combines with any mode (and with the default).
    std::optional<std::string> saveScenePath;     // --save-scene=<path>
};

// Parses argv into `out`. Returns false on invalid input, after logging a
// CRITICAL diagnostic (never a silent false-green); `out` is unspecified on
// failure. Behaviour is the CLI contract of ADR-0010 Q8 / ticket 0024: unknown
// flags, empty `--asset=` and non-numeric/negative `--frames=` are all rejected.
bool parseOptions(int argc, char** argv, Options& out);

}  // namespace noto3d::app
