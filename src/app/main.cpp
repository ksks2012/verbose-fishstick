#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <gl_capability.hpp>
#include <log.hpp>
#include <material.hpp>
#include <mesh.hpp>
#include <shader_program.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <stdexcept>

#ifndef NOTO3D_SHADER_DIR
#define NOTO3D_SHADER_DIR "./shaders"
#endif

#ifndef NOTO3D_DEBUG_GL
#define NOTO3D_DEBUG_GL 0
#endif

namespace {

const char* kWindowTitle  = "noto3d-editor (M1)";
const int   kInitialW     = 1280;
const int   kInitialH     = 720;
const float kClearLinear[4] = {0.12f, 0.12f, 0.14f, 1.0f};  // linear-space clear (ADR-0005)

// Cube test asset — vertices verbatim from assets/test/cube.obj (ADR-0001).
// 8 interleaved Vertex entries: position/normal/uv.
// Normals are vertex-corner-averaged (gen_placeholders.py output); NOT face-normal
// aligned — the M1 fragment stage does not use them.
// TODO(0008): this inline fixture is superseded when the M2 loader lands.
const noto3d::renderer::Vertex kCubeVerts[] = {
    //  position                  normal                              uv
    {{ 0.5f, 0.5f, 0.5f},  { 0.333333f, 0.666667f, 0.666667f},  {0.5f, 0.5f}},  // 0
    {{ 0.5f,-0.5f, 0.5f},  { 0.408248f,-0.816497f, 0.408248f},  {0.5f, 0.5f}},  // 1
    {{ 0.5f,-0.5f,-0.5f},  { 0.577350f,-0.577350f,-0.577350f},  {0.5f, 0.5f}},  // 2
    {{ 0.5f, 0.5f,-0.5f},  { 0.000000f, 0.447214f,-0.894427f},  {0.5f, 0.5f}},  // 3
    {{-0.5f, 0.5f, 0.5f},  {-0.816497f, 0.408248f, 0.408248f},  {0.5f, 0.5f}},  // 4
    {{-0.5f, 0.5f,-0.5f},  {-0.408248f, 0.816497f,-0.408248f},  {0.5f, 0.5f}},  // 5
    {{-0.5f,-0.5f,-0.5f},  {-0.577350f,-0.577350f,-0.577350f},  {0.5f, 0.5f}},  // 6
    {{-0.5f,-0.5f, 0.5f},  {-0.408248f,-0.408248f, 0.816497f},  {0.5f, 0.5f}},  // 7
};
// Index array — verbatim from cube.obj face data (0-based).
// Triangle 0 (0,0,1) is degenerate (zero area); gen_placeholders.py artifact.
// TODO(0008): 0008 loader canonicalizes geometry; this fixture is then superseded.
const std::uint32_t kCubeIdx[] = {
    0,0,1,  0,1,2,   // right (+X)
    4,5,6,  4,6,7,   // left  (-X)
    0,3,5,  0,5,4,   // top   (+Y)
    1,7,6,  1,6,2,   // bottom(-Y)
    0,4,7,  0,7,1,   // front (+Z)
    3,2,6,  3,6,5,   // back  (-Z)
};

void glfwErrorCallback(int code, const char* desc) {
    NOTO3D_ERROR("[glfw] error {}: {}", code, desc);
}

void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    glViewport(0, 0, width, height);  // DPI-safe viewport resize (ADR-0004)
}

#if NOTO3D_DEBUG_GL
void GLAPIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                              GLsizei /*length*/, const char* message,
                              const void* /*userParam*/) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;  // filter NOTIFICATION (ADR-0004)
    }
    const char* sev = (severity == GL_DEBUG_SEVERITY_HIGH)   ? "HIGH"
                    : (severity == GL_DEBUG_SEVERITY_MEDIUM) ? "MEDIUM"
                                                             : "LOW";
    NOTO3D_ERROR("[gl-debug][{}] id={} src={:#x} type={:#x}: {}", sev, id,
                 static_cast<unsigned>(source), static_cast<unsigned>(type), message);
}
#endif

}  // namespace

static int run(int frameLimit) {
    if (!glfwInit()) {
        NOTO3D_CRITICAL("M0_FAIL: glfwInit failed");
        return EXIT_FAILURE;
    }
    glfwSetErrorCallback(glfwErrorCallback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
#if NOTO3D_DEBUG_GL
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(kInitialW, kInitialH, kWindowTitle, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        NOTO3D_CRITICAL("M0_FAIL: window/context creation failed");
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);  // DPI-safe resize (ADR-0004)

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
        NOTO3D_CRITICAL("M0_FAIL: glad load failed");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ---- Numeric capability validation (ADR-0004) -------------------------
    GLint glMajor = 0;
    GLint glMinor = 0;
    GLint profile = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
    glGetIntegerv(GL_MINOR_VERSION, &glMinor);
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);

    const bool capOk   = noto3d::core::supportsRequiredContext(glMajor, glMinor, profile);
    const bool funcsOk = glCreateShader != nullptr && glGenVertexArrays != nullptr &&
                         glClear != nullptr && glDrawElements != nullptr;

    // Diagnostics (strings are diagnostic only, never parsed)
    NOTO3D_INFO("M0_INFO  version-string : {}",
                reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    NOTO3D_INFO("M0_INFO  renderer       : {}",
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    NOTO3D_INFO("M0_INFO  glsl-string    : {}",
                reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
    NOTO3D_INFO("M0_INFO  numeric        : GL {}.{} core-profile={}", glMajor, glMinor,
                (profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0 ? 1 : 0);

    if (!capOk || !funcsOk) {
        NOTO3D_CRITICAL(
            "M0_FAIL: capability check failed version={}.{} core={} funcs={} "
            "(require OpenGL 3.3 Core)",
            glMajor, glMinor,
            (profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0 ? 1 : 0,
            funcsOk ? 1 : 0);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // ---- Renderer state baseline (ADR-0005) -------------------------------
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(kClearLinear[0], kClearLinear[1], kClearLinear[2], kClearLinear[3]);

#if NOTO3D_DEBUG_GL
    if (GLAD_GL_KHR_debug && glDebugMessageCallback != nullptr) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugCallback, nullptr);
    } else {
        NOTO3D_WARN("M0_WARN  debug context requested but GL_KHR_debug unavailable");
    }
#endif

    NOTO3D_INFO("M0_OK context 3.3+ core profile; renderer baseline configured");

    // ---- M1 — base shader + cube test asset (ADR-0001/0006) ---------------
    // Both steps in one try block: shader throws on GLSL error; mesh upload is
    // checked via glGetError (GL does not throw on OOM).
    std::optional<noto3d::renderer::ShaderProgram> baseShader;
    std::optional<noto3d::renderer::Mesh>          cubeMesh;
    try {
        baseShader.emplace(noto3d::renderer::ShaderProgram::fromFiles(
            NOTO3D_SHADER_DIR "/base.vert",
            NOTO3D_SHADER_DIR "/base.frag"));
        cubeMesh.emplace(kCubeVerts, kCubeIdx);
        if (const GLenum glErr = glGetError(); glErr != GL_NO_ERROR) {
            NOTO3D_ERROR("GL error {:#010x} after mesh upload", static_cast<unsigned>(glErr));
            throw std::runtime_error("GL error after mesh upload");
        }
    } catch (const std::exception& e) {
        NOTO3D_CRITICAL("M1_FAIL: setup failed — {}", e.what());
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    const noto3d::renderer::Material cubeMat{};  // defaults: white, opacity=1, cull=true
    NOTO3D_INFO("M1_OK cube mesh uploaded; base shader compiled");

    // ---- Render loop -------------------------------------------------------
    int frames = 0;
    while (!glfwWindowShouldClose(window) && (frameLimit < 0 || frames < frameLimit)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---- Draw cube (ADR-0001 explicit test asset) ----------------------
        {
            int fbW = 0, fbH = 0;
            glfwGetFramebufferSize(window, &fbW, &fbH);
            const float aspect = (fbH > 0)
                ? static_cast<float>(fbW) / static_cast<float>(fbH) : 1.0f;
            const auto proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
            const auto view = glm::lookAt(glm::vec3(0.0f, 0.5f, 3.0f),
                                          glm::vec3(0.0f, 0.0f, 0.0f),
                                          glm::vec3(0.0f, 1.0f, 0.0f));
            const auto mvp  = proj * view;  // model = identity

            // TODO(0007): cull-state toggle is a temporary per-draw workaround.
            // 0007 Begin3DPass/End3DPass will own canonical state for the whole pass
            // and handle per-material cullEnabled within a proper pass boundary.
            if (!cubeMat.cullEnabled) { glDisable(GL_CULL_FACE); }
            baseShader->use();
            baseShader->setMat4 ("uMVP",        mvp);
            baseShader->setVec4 ("uBaseColor",  cubeMat.baseColor);
            baseShader->setFloat("uOpacity",    cubeMat.opacity);
            // Texture: bind to unit 0 if present; set uHasTexture accordingly (0009)
            if (cubeMat.texture.has_value()) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, cubeMat.texture.value());
                baseShader->setInt("uHasTexture", 1);
                baseShader->setInt("uDiffuse",    0);
            } else {
                baseShader->setInt("uHasTexture", 0);
            }
            glBindVertexArray(cubeMesh->vao());
            glDrawElements(GL_TRIANGLES, cubeMesh->indexCount(), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
            if (!cubeMat.cullEnabled) { glEnable(GL_CULL_FACE); }
        }
        glfwSwapBuffers(window);
        ++frames;
    }

    NOTO3D_INFO("M1_DONE rendered {} frames", frames);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}

int main(int argc, char** argv) {
    noto3d::core::log_init();

    int frameLimit = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--frames=", 9) == 0) {
            frameLimit = std::atoi(argv[i] + 9);
        }
    }
    return run(frameLimit);
}