#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <gl_capability.hpp>
#include <log.hpp>

#include <cstdlib>
#include <cstring>

#ifndef NOTO3D_DEBUG_GL
#define NOTO3D_DEBUG_GL 0
#endif

namespace {

const char* kWindowTitle  = "noto3d-editor (M0)";
const int   kInitialW     = 1280;
const int   kInitialH     = 720;
const float kClearLinear[4] = {0.12f, 0.12f, 0.14f, 1.0f};  // linear-space clear (ADR-0005)

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

    // ---- Render loop -------------------------------------------------------
    int frames = 0;
    while (!glfwWindowShouldClose(window) && (frameLimit < 0 || frames < frameLimit)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glfwSwapBuffers(window);
        ++frames;
    }

    NOTO3D_INFO("M0_DONE rendered {} frames", frames);
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