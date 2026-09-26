#pragma once

#include "mesh.hpp"
#include "material.hpp"
#include "shader_program.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace noto3d::renderer {

// Renderer owns the 3D pass state contract (ADR-0005/0008).
//
// Usage per frame:
//   renderer.Begin3DPass();           // canonical state + clear
//   renderer.submit(mesh, mat, sh, mvp);  // one draw call per mesh
//   renderer.End3DPass();             // pass boundary marker
//
// ADR-0005 pass hygiene:
//   * States are set once per pass (Begin3DPass), not per draw call.
//   * Material::cullEnabled is the only per-draw escape hatch.
//   * recorded_winding is diagnostic only — never flips GL state.
//   * M5 concern: ImGui overlay owns and restores its own GL state;
//     End3DPass() is the handoff point (no-op at M1).
class Renderer {
public:
    // Linear-space clear colour owned by the renderer (ADR-0005).
    // Never authored as sRGB — the sRGB framebuffer converts on output.
    static constexpr float kClearR = 0.12f;
    static constexpr float kClearG = 0.12f;
    static constexpr float kClearB = 0.14f;

    // Establish the full ADR-0005 canonical 3D state block and clear buffers.
    // Must be called exactly once per frame, before any submit() calls.
    void Begin3DPass() noexcept;

    // Terminate the 3D pass.
    // M1: no-op boundary marker.
    // M5: this is where the ImGui pass takes ownership of GL state (ADR-0005).
    void End3DPass() noexcept;

    // Submit one draw call through shader with the given MVP matrix.
    // Honors Material::cullEnabled per draw (ADR-0005 per-draw escape hatch).
    // Binds diffuse texture to unit 0 when Material::texture is present (ADR-0004/0009).
    void submit(const Mesh&       mesh,
                const Material&   mat,
                ShaderProgram&    shader,
                const glm::mat4&  mvp);
};

}  // namespace noto3d::renderer
