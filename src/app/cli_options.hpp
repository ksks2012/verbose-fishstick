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
};

// Parses argv into `out`. Returns false on invalid input, after logging a
// CRITICAL diagnostic (never a silent false-green); `out` is unspecified on
// failure. Behaviour is the CLI contract of ADR-0010 Q8 / ticket 0024: unknown
// flags, empty `--asset=` and non-numeric/negative `--frames=` are all rejected.
bool parseOptions(int argc, char** argv, Options& out);

}  // namespace noto3d::app
