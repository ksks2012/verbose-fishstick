#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "texture.hpp"
#include "log.hpp"

#include <stdexcept>
#include <string>

namespace noto3d::renderer {

// Compile-time pin of the diffuse upload format (ticket 0009) — same style as the
// Vertex stride invariant at vertex.hpp. Verifies what the app requests, not driver storage.
static_assert(Texture::kDiffuseInternalFormat == GL_SRGB8_ALPHA8,
              "diffuse textures must upload as GL_SRGB8_ALPHA8 (ADR-0004)");

Texture Texture::fromFile(const std::filesystem::path& path) {
    stbi_set_flip_vertically_on_load(1);  // OpenGL bottom-left origin

    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!data) {
        throw std::runtime_error(
            "[texture] stbi_load failed: " + path.string() +
            " (" + stbi_failure_reason() + ")");
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // Upload with sRGB internal format — hardware decodes sRGB→linear on sample (ADR-0004)
    glTexImage2D(GL_TEXTURE_2D, 0,
                 static_cast<GLint>(kDiffuseInternalFormat),
                 w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Filter policy: linear + mipmaps (independent of MSAA, ADR-0004)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, kMinFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kMagFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    NOTO3D_DEBUG("[texture] uploaded '{}' ({}x{}, {} ch → SRGB8_ALPHA8)",
                 path.filename().string(), w, h, channels);
    return Texture(id);
}

Texture::Texture(GLuint id) noexcept : m_id(id) {}

Texture::~Texture() { release(); }

Texture::Texture(Texture&& other) noexcept : m_id(other.m_id) {
    other.m_id = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        release();
        m_id       = other.m_id;
        other.m_id = 0;
    }
    return *this;
}

void Texture::bind(GLenum unit) const noexcept {
    glActiveTexture(unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::release() noexcept {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
}

}  // namespace noto3d::renderer
