#include "mesh.hpp"
#include "log.hpp"

#include <cstddef>
#include <cstdlib>
#include <type_traits>

// ---------------------------------------------------------------------------
// Compile-time contract checks (ADR-0006)
// ---------------------------------------------------------------------------
static_assert(sizeof(noto3d::renderer::Vertex) == 32,
              "Vertex stride must be 32 B");
static_assert(offsetof(noto3d::renderer::Vertex, position) == 0,
              "Vertex::position must be at offset 0");
static_assert(offsetof(noto3d::renderer::Vertex, normal) == 12,
              "Vertex::normal must be at offset 12");
static_assert(offsetof(noto3d::renderer::Vertex, uv) == 24,
              "Vertex::uv must be at offset 24");

static_assert(!std::is_copy_constructible_v<noto3d::renderer::Mesh>,
              "Mesh must not be copy-constructible");
static_assert(!std::is_copy_assignable_v<noto3d::renderer::Mesh>,
              "Mesh must not be copy-assignable");
static_assert(std::is_move_constructible_v<noto3d::renderer::Mesh>,
              "Mesh must be move-constructible");
static_assert(std::is_move_assignable_v<noto3d::renderer::Mesh>,
              "Mesh must be move-assignable");

namespace {

int g_failures = 0;

void check(bool cond, const char* name) {
    if (cond) {
        NOTO3D_INFO("PASS  {}", name);
    } else {
        NOTO3D_ERROR("FAIL  {}", name);
        ++g_failures;
    }
}

}  // namespace

int main() {
    noto3d::core::log_init("noto3d-renderer-tests");

    using noto3d::renderer::Mesh;
    using noto3d::renderer::Vertex;

    // Vertex layout (runtime mirrors of the compile-time asserts above)
    check(sizeof(Vertex) == 32,                  "sizeof(Vertex) == 32");
    check(offsetof(Vertex, position) == 0,       "Vertex::position at offset 0");
    check(offsetof(Vertex, normal) == 12,        "Vertex::normal at offset 12");
    check(offsetof(Vertex, uv) == 24,            "Vertex::uv at offset 24");

    // Move / copy traits
    check(!std::is_copy_constructible_v<Mesh>,   "Mesh is not copy-constructible");
    check(!std::is_copy_assignable_v<Mesh>,      "Mesh is not copy-assignable");
    check(std::is_move_constructible_v<Mesh>,    "Mesh is move-constructible");
    check(std::is_move_assignable_v<Mesh>,       "Mesh is move-assignable");

    if (g_failures == 0) {
        NOTO3D_INFO("ALL PASS");
        return EXIT_SUCCESS;
    }
    NOTO3D_ERROR("{} FAILURE(S)", g_failures);
    return EXIT_FAILURE;
}
