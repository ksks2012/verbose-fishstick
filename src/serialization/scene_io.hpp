#pragma once

// ADR-0008 declaration-only-header rule: constants with external linkage are
// declared here and defined in scene_io.cpp.  The serialization module does
// not include asset, renderer, or UI headers — the architecture guard in
// CMakeLists.txt enforces this at configure time (ticket 0041).

namespace noto3d::serialization {

// Version of the JSON document format produced and consumed by this module.
// Consumers must check this field before reading any other key.
// Increment this when the on-disk format changes in a backward-incompatible way.
extern const int kSceneFormatVersion;

}  // namespace noto3d::serialization
