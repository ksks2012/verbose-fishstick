#include "cli_options.hpp"

#include "log.hpp"

#include <cstring>
#include <exception>
#include <string>

namespace noto3d::app {

// Parses --frames=N (validated, ticket 0011-⑨), --asset=<id>, --demo, --list-assets.
// Returns false → caller exits non-zero with a diagnostic (never a silent false-green).
bool parseOptions(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strncmp(arg, "--frames=", 9) == 0) {
            const char*    num      = arg + 9;
            const std::string token(num);
            std::size_t    consumed = 0;
            int            v        = 0;
            try {
                v = std::stoi(token, &consumed);
            } catch (const std::exception&) {
                v = -1;  // non-numeric → caught by the v < 0 check below
            }
            if (consumed != token.size() || v < 0) {
                NOTO3D_CRITICAL("--frames= expects a non-negative integer, got '{}'", num);
                return false;
            }
            out.frameLimit = v;
        } else if (std::strncmp(arg, "--asset=", 8) == 0) {
            const char* id = arg + 8;
            if (*id == '\0') {
                NOTO3D_CRITICAL("--asset= requires a manifest asset id");
                return false;
            }
            out.assetId = id;
        } else if (std::strcmp(arg, "--demo") == 0) {
            out.demo = true;
        } else if (std::strcmp(arg, "--list-assets") == 0) {
            out.listAssets = true;
        } else {
            // Unknown/unsupported flag: rejection is a CLI contract (ADR-0010 Q8
            // "invalid flag → exit 1"; ticket 0024 matrix). Never silently ignore.
            NOTO3D_CRITICAL("unknown option '{}'", arg);
            return false;
        }
    }
    return true;
}

}  // namespace noto3d::app
