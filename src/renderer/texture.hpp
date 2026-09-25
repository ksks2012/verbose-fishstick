#pragma once

#include <glad/gl.h>

#include <filesystem>

namespace noto3d::renderer {

// RAII GL texture.  Non-copyable, movable.
// Always uploads as a diffuse (color) texture using the sRGB internal format
// so hardware sampling returns linear values into the linear pipeline (ADR-0004).
class Texture {
public:
    // Internal format for diffuse color textures — hardware sRGB decode on sample (ADR-0004).
    static constexpr GLenum kDiffuseInternalFormat = GL_SRGB8_ALPHA8;

    // Minification and magnification filter policy, chosen independently of MSAA (ADR-0004).
    static constexpr GLint kMinFilter = GL_LINEAR_MIPMAP_LINEAR;
    static constexpr GLint kMagFilter = GL_LINEAR;

    // Load a PNG via stb_image and upload with kDiffuseInternalFormat + mipmap chain.
    // Throws std::runtime_error if the file cannot be opened or decoded.
    static Texture fromFile(const std::filesystem::path& path);

    ~Texture();
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;

    // Bind to a texture unit (default: GL_TEXTURE0).
    void bind(GLenum unit = GL_TEXTURE0) const noexcept;

    [[nodiscard]] GLuint id() const noexcept { return m_id; }

private:
    explicit Texture(GLuint id) noexcept;
    void release() noexcept;

    GLuint m_id = 0;
};

}  // namespace noto3d::renderer
