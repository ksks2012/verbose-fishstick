#include "scene_io.hpp"

// ADR-0008 declaration-only-header rule: this definition lives here, never
// in the header.  The header's `extern const int` makes the linkage external;
// writing `const int kSceneFormatVersion = 1;` after including it would
// otherwise give the object internal linkage and break every consumer.
//
// The serializer sees only src/scene: no asset, renderer, or UI headers.  The
// architecture guard in CMakeLists.txt enforces that at configure time
// (ticket 0041, ADR-0013 Q11), so this file stays free of GL/windowing types
// by construction rather than by discipline.

namespace noto3d::serialization {

const int kSceneFormatVersion = 1;

}  // namespace noto3d::serialization
