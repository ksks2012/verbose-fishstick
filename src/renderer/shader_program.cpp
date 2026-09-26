#include "shader_program.hpp"
#include "log.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace noto3d::renderer {

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("[shader] cannot open: " + path.string());
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint compileShader(GLenum type, const std::string& src,
                     const std::filesystem::path& path) {
    GLuint shader = glCreateShader(type);
    const char* cstr = src.c_str();
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<std::size_t>(len > 0 ? len : 1));
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("[shader] compile error in " + path.string() +
                                 ":\n" + log.data());
    }
    return shader;
}

}  // namespace

ShaderProgram ShaderProgram::fromFiles(const std::filesystem::path& vertPath,
                                       const std::filesystem::path& fragPath) {
    const std::string vertSrc = readFile(vertPath);
    const std::string fragSrc = readFile(fragPath);

    const GLuint vert = compileShader(GL_VERTEX_SHADER,   vertSrc, vertPath);
    const GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);

    const GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<std::size_t>(len > 0 ? len : 1));
        glGetProgramInfoLog(program, len, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("[shader] link error:\n" + std::string(log.data()));
    }

    NOTO3D_DEBUG("[shader] linked program {} ({} + {})", program,
                 vertPath.filename().string(), fragPath.filename().string());
    return ShaderProgram(program);
}

ShaderProgram::ShaderProgram(GLuint program) noexcept
    : m_program(program) {}

ShaderProgram::~ShaderProgram() {
    release();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : m_program(other.m_program),
      m_uniformCache(std::move(other.m_uniformCache)) {
    other.m_program = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        release();
        m_program       = other.m_program;
        other.m_program = 0;
        m_uniformCache  = std::move(other.m_uniformCache);
    }
    return *this;
}

void ShaderProgram::use() const noexcept {
    glUseProgram(m_program);
}

GLint ShaderProgram::uniformLocation(const char* name) const {
    const std::string key(name);
    if (const auto hit = m_uniformCache.find(key); hit != m_uniformCache.end()) {
        return hit->second;
    }
    const GLint loc = glGetUniformLocation(m_program, name);
    m_uniformCache.emplace(key, loc);
    return loc;
}

void ShaderProgram::setMat4(const char* name, const glm::mat4& v) const {
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, &v[0][0]);
}

void ShaderProgram::setVec4(const char* name, const glm::vec4& v) const {
    glUniform4fv(uniformLocation(name), 1, &v[0]);
}

void ShaderProgram::setFloat(const char* name, float v) const {
    glUniform1f(uniformLocation(name), v);
}

void ShaderProgram::setInt(const char* name, int v) const {
    glUniform1i(uniformLocation(name), v);
}

void ShaderProgram::release() noexcept {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_uniformCache.clear();
}

}  // namespace noto3d::renderer
