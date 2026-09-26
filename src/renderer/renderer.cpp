#include "renderer.hpp"
#include "log.hpp"

namespace noto3d::renderer {

void Renderer::Begin3DPass() noexcept {
    // Canonical 3D pass state — set once per pass, not per draw call (ADR-0005).
    glEnable(GL_MULTISAMPLE);          // MSAA 4x (ADR-0004)
    glEnable(GL_FRAMEBUFFER_SRGB);     // linear → sRGB conversion on output (ADR-0004)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);              // depth writes on (ADR-0005)
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);               // placeholder geometry is CCW-outward (ADR-0005)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // straight alpha (ADR-0005)

    // Clear color in linear RGB; sRGB framebuffer converts on output (ADR-0005)
    glClearColor(kClearR, kClearG, kClearB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::End3DPass() noexcept {
    // M1: no-op boundary marker — no GL snapshot/restore machinery at this milestone.
    // M5 responsibility (ADR-0005): ImGui overlay owns and restores its own GL state;
    // End3DPass() will signal the handoff from the 3D pass to the ImGui pass.
}

void Renderer::submit(const Mesh&       mesh,
                      const Material&   mat,
                      ShaderProgram&    shader,
                      const glm::mat4&  mvp) {
    // Per-material cullEnabled escape hatch (ADR-0005).
    // recorded_winding is diagnostic metadata only — it never flips GL state.
    if (!mat.cullEnabled) { glDisable(GL_CULL_FACE); }

    shader.use();
    shader.setMat4 ("uMVP",       mvp);
    shader.setVec4 ("uBaseColor", mat.baseColor);
    shader.setFloat("uOpacity",   mat.opacity);

    // Bind diffuse texture to unit 0 when present (ADR-0004/0009)
    if (mat.texture.has_value()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mat.texture.value());
        shader.setInt("uHasTexture", 1);
        shader.setInt("uDiffuse",    0);
    } else {
        shader.setInt("uHasTexture", 0);
    }

    glBindVertexArray(mesh.vao());
    glDrawElements(GL_TRIANGLES, mesh.indexCount(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    // Restore cull state if it was disabled for this draw
    if (!mat.cullEnabled) { glEnable(GL_CULL_FACE); }
}

}  // namespace noto3d::renderer
