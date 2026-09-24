#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <filesystem>

namespace noto3d::renderer {

// RAII wrapper around a linked GL shader program.
// Non-copyable, movable.  Throws std::runtime_error on compile/link failure.
class ShaderProgram {
public:
    // Compile and link shaders from source files.
    // Throws std::runtime_error if a file cannot be opened or if compilation/linking fails.
    static ShaderProgram fromFiles(const std::filesystem::path& vertPath,
                                   const std::filesystem::path& fragPath);

    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&)            = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) noexcept;
    ShaderProgram& operator=(ShaderProgram&&) noexcept;

    void use() const noexcept;

    void setMat4 (const char* name, const glm::mat4& v) const;
    void setVec4 (const char* name, const glm::vec4& v) const;
    void setFloat(const char* name, float v)             const;

private:
    explicit ShaderProgram(GLuint program) noexcept;
    void release() noexcept;

    GLuint m_program = 0;
};

}  // namespace noto3d::renderer
