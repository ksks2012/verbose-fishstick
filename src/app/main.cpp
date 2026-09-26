#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <asset_registry.hpp>
#include <gl_capability.hpp>
#include <log.hpp>
#include <renderer.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>

// Shader + asset locations are owned by the build system, never discovered at
// runtime (ticket 0011-⑦): CMake defines both absolute paths from the source dir.
#ifndef NOTO3D_SHADER_DIR
#error "NOTO3D_SHADER_DIR must be defined by the build system"
#endif

#ifndef NOTO3D_ASSETS_DIR
#error "NOTO3D_ASSETS_DIR must be defined by the build system"
#endif

#ifndef NOTO3D_DEBUG_GL
#define NOTO3D_DEBUG_GL 0
#endif

namespace {

const char* kWindowTitle = "noto3d-editor (M2)";
const int   kInitialW    = 1280;
const int   kInitialH    = 720;

// Command-line options for the M2 demo runner.
struct Options {
    int frameLimit = -1;                  // < 0 = unlimited
    std::optional<std::string> assetId;   // manifest asset to render (default: first renderable)
    bool  demo   = false;                 // also render the 0014 demo_checker fixture (textured)
};

// Parses --frames=N (validated, ticket 0011-⑨), --asset=<id>, --demo.
// Returns false → caller exits non-zero with a diagnostic (never a silent false-green).
bool parseOptions(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strncmp(arg, "--frames=", 9) == 0) {
            const char*    num      = arg + 9;
            const std::string token(num);
            std::size_t    consumed = 0;
            int            v        = 0;
            try {
                v = std::stoi(token, &consumed);
            } catch (const std::exception&) {
                v = -1;  // non-numeric → caught by the v < 0 check below
            }
            if (consumed != token.size() || v < 0) {
                NOTO3D_CRITICAL("--frames= expects a non-negative integer, got '{}'", num);
                return false;
            }
            out.frameLimit = v;
        } else if (std::strncmp(arg, "--asset=", 8) == 0) {
            const char* id = arg + 8;
            if (*id == '\0') {
                NOTO3D_CRITICAL("--asset= requires a manifest asset id");
                return false;
            }
            out.assetId = id;
        } else if (std::strcmp(arg, "--demo") == 0) {
            out.demo = true;
        }
    }
    return true;
}

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

// Shader setup is app-owned but created here so the render loop never juggles
// optionals/pointers (ticket 0011-⑧).  A shader that cannot compile is fatal for
// this process; the caller tears the window down and returns failure.
bool loadBaseShader(noto3d::renderer::ShaderProgram& out) {
    try {
        out = noto3d::renderer::ShaderProgram::fromFiles(
            NOTO3D_SHADER_DIR "/base.vert",
            NOTO3D_SHADER_DIR "/base.frag");
        return true;
    } catch (const std::exception& e) {
        NOTO3D_CRITICAL("M1_FAIL: shader setup failed — {}", e.what());
        return false;
    }
}

}  // namespace

static int run(const Options& opts) {
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

    NOTO3D_INFO("M0_OK context 3.3+ core profile; renderer baseline configured");

    // Debug GL context setup — one-time, not a per-pass concern (ADR-0004)
#if NOTO3D_DEBUG_GL
    if (GLAD_GL_KHR_debug && glDebugMessageCallback != nullptr) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugCallback, nullptr);
    } else {
        NOTO3D_WARN("M0_WARN  debug context requested but GL_KHR_debug unavailable");
    }
#endif

    // ---- Scene scope --------------------------------------------------------
    // All GL-owning objects (shader, registry → Mesh/Texture, renderer) live in
    // this scope so they are destroyed while the context is still current —
    // never after glfwDestroyWindow/glfwTerminate (which would delete GL objects
    // without a context).
    int  exitCode = EXIT_SUCCESS;
    int  frames   = 0;
    bool ranLoop  = false;
    {
        noto3d::renderer::ShaderProgram baseShader;
        noto3d::asset::AssetRegistry    registry;
        noto3d::renderer::Renderer      renderer;

        auto fail = [&](const char* msg) {
            NOTO3D_CRITICAL("{}", msg);
            exitCode = EXIT_FAILURE;
            return false;
        };

        bool ok = true;

        // ---- M1 — base shader (ticket 0011-⑧: no optional wrapper, no inline cube)
        ok = loadBaseShader(baseShader);
        if (!ok) {
            // M1_FAIL already logged; skip straight to scope teardown.
        } else {
            NOTO3D_INFO("M1_OK base shader compiled; assets load via AssetRegistry");

            // ---- M2 — manifest assets through the AssetRegistry (ticket 0012)
            const auto loadR =
                registry.load(std::string(NOTO3D_ASSETS_DIR) + "/manifest.json");
            for (const auto& d : loadR.diagnostics) {
                NOTO3D_WARN("M2_WARN  [{}] {}",
                            d.assetId.empty() ? "manifest" : d.assetId, d.message);
            }
            NOTO3D_INFO("M2_INFO  manifest loaded: {} ok, {} warned, {} failed",
                        loadR.loaded, loadR.warned, loadR.failed);

            // Select the asset to render: explicit id wins, else first renderable.
            const noto3d::renderer::Model* sceneModel = nullptr;
            if (opts.assetId) {
                const noto3d::asset::AssetEntry* entry = registry.findEntry(*opts.assetId);
                if (!entry) {
                    ok = fail(("M2_FAIL: unknown asset '" + *opts.assetId + "'").c_str());
                } else {
                    sceneModel = registry.model(entry->id);
                    if (!sceneModel) {
                        NOTO3D_WARN("M2_WARN  asset '{}' has no GPU geometry "
                                    "(geometry-less or failed)", entry->id);
                    }
                }
            } else {
                for (const auto& e : registry.entries()) {
                    if (const auto* m = registry.model(e.id)) {
                        sceneModel = m;
                        NOTO3D_INFO("M2_INFO  rendering manifest asset '{}'", e.id);
                        break;
                    }
                }
                if (!sceneModel) {
                    NOTO3D_WARN("M2_WARN  no manifest asset is renderable; using demo only");
                }
            }

            // --demo: render the 0014 fixture through the registry's standalone path.
            // Closes 0009's E2E gate: diffuseTextureRef → Texture::fromFile → render.
            const noto3d::renderer::Model* demoModel = nullptr;
            if (opts.demo) {
                demoModel = registry.loadModelFile(
                    std::string(NOTO3D_ASSETS_DIR) +
                        "/geometry/placeholders/demo_checker/demo_checker.obj",
                    "demo_checker");
                bool textured = false;
                if (demoModel) {
                    for (const auto& sm : *demoModel) {
                        textured = textured || sm.material.texture.has_value();
                    }
                }
                if (demoModel && textured) {
                    NOTO3D_INFO("M2_OK   demo_checker fixture rendered with diffuse "
                                "texture (0009 E2E)");
                } else if (demoModel) {
                    NOTO3D_WARN("M2_WARN  demo_checker fixture rendered WITHOUT a "
                                "diffuse texture");
                } else {
                    ok = fail("M2_FAIL: demo fixture failed to load");
                }
            }

            if (ok && !sceneModel && !demoModel) {
                ok = fail("M2_FAIL: nothing to render");
            }

            // ---- Render loop ---------------------------------------------------
            if (ok) {
                while (!glfwWindowShouldClose(window) &&
                       (opts.frameLimit < 0 || frames < opts.frameLimit)) {
                    glfwPollEvents();
                    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                    }

                    // Compute MVP for this frame
                    int fbW = 0, fbH = 0;
                    glfwGetFramebufferSize(window, &fbW, &fbH);
                    const float aspect = (fbH > 0)
                        ? static_cast<float>(fbW) / static_cast<float>(fbH) : 1.0f;
                    const auto proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
                    const auto view = glm::lookAt(glm::vec3(0.0f, 0.5f, 3.0f),
                                                  glm::vec3(0.0f, 0.0f, 0.0f),
                                                  glm::vec3(0.0f, 1.0f, 0.0f));
                    const auto mvp = proj * view;  // model = identity

                    renderer.Begin3DPass();
                    const auto submitModel = [&](const noto3d::renderer::Model& model) {
                        for (const auto& sm : model) {
                            renderer.submit(sm.mesh, sm.material, baseShader, mvp);
                        }
                    };
                    if (sceneModel) submitModel(*sceneModel);
                    if (demoModel)  submitModel(*demoModel);
                    renderer.End3DPass();

                    glfwSwapBuffers(window);
                    ++frames;
                }
                ranLoop = true;
            }
        }
    }

    // Scene scope closed: every GL object is freed while the context was current.
    if (ranLoop) NOTO3D_INFO("M1_DONE rendered {} frames", frames);
    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}

int main(int argc, char** argv) {
    noto3d::core::log_init();

    Options opts;
    if (!parseOptions(argc, argv, opts)) {
        return EXIT_FAILURE;
    }
    return run(opts);
}