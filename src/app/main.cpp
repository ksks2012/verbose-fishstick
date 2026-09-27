#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <asset_registry.hpp>
#include <draw_list.hpp>
#include <gl_capability.hpp>
#include <log.hpp>
#include <renderer.hpp>
#include <scene.hpp>

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

const char* kWindowTitle = "noto3d (M3)";
const int   kInitialW    = 1280;
const int   kInitialH    = 720;

// Command-line options for the demo runner.
struct Options {
    int frameLimit = -1;                  // < 0 = unlimited
    std::optional<std::string> assetId;   // manifest asset to render (default: first renderable)
    bool  demo   = false;                 // also render the 0014 demo_checker fixture (textured)
    bool  listAssets = false;             // diagnostic CLI: print manifest assets, exit pre-GL
};

// Parses --frames=N (validated, ticket 0011-⑨), --asset=<id>, --demo, --list-assets.
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
        } else if (std::strcmp(arg, "--list-assets") == 0) {
            out.listAssets = true;
        }
    }
    return true;
}

const char* geometrySourceName(const noto3d::asset::AssetEntry& e) {
    using noto3d::asset::GeometrySource;
    if (!e.geometrySource) return "null";
    return *e.geometrySource == GeometrySource::Placeholder ? "placeholder" : "official";
}

const char* referenceName(const noto3d::asset::AssetEntry& e) {
    return e.referenceImage ? e.referenceImage->c_str() : "(none)";
}

// ---- C — --list-assets diagnostic CLI (ticket 0017) ------------------------
// Loads the manifest to CPU only (no GL context), prints id / geometry source /
// reference per asset, then exits 0.  Not a new asset-pipeline capability: it is
// the existing loadManifestOnly + entries() path, used as a pre-GL diagnostic.
static int listAssetsCommand() {
    noto3d::asset::AssetRegistry registry;
    const auto loadR = registry.loadManifestOnly(
        std::string(NOTO3D_ASSETS_DIR) + "/manifest.json");
    for (const auto& d : loadR.diagnostics) {
        NOTO3D_WARN("M3_WARN  [{}] {}",
                    d.assetId.empty() ? "manifest" : d.assetId, d.message);
    }
    NOTO3D_INFO("M3_ASSETS version=1");
    for (const auto& e : registry.entries()) {
        NOTO3D_INFO("M3_ASSETS id='{}' geometry={} reference={}",
                    e.id, geometrySourceName(e), referenceName(e));
    }
    NOTO3D_INFO("M3_ASSETS end count={}", static_cast<int>(registry.entries().size()));
    return EXIT_SUCCESS;
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

            // ---- M4 — Scene construction (ADR-0010 Q5/Q6) -------------------------
            // Build a Scene for the selected entry mode; every render path goes through
            // collectDrawItems() — no direct submit bypass (ticket 0022).
            // --asset and --demo are mutually exclusive (ticket 0022 CLI contract).
            noto3d::scene::Scene      scene;
            std::string               sceneName;
            noto3d::app::Resolver     resolver;

            if (opts.assetId && opts.demo) {
                ok = fail("M4_FAIL: --asset and --demo are mutually exclusive");

            } else if (opts.demo) {
                // Demo-only Scene: exactly one entity, identity transform (0014 fixture).
                const noto3d::renderer::Model* demoPtr =
                    registry.loadModelFile(
                        std::string(NOTO3D_ASSETS_DIR) +
                            "/geometry/placeholders/demo_checker/demo_checker.obj",
                        "demo_checker");
                if (!demoPtr) {
                    ok = fail("M2_FAIL: demo fixture failed to load");
                } else {
                    bool textured = false;
                    for (const auto& sm : *demoPtr) {
                        textured = textured || sm.material.texture.has_value();
                    }
                    if (textured) {
                        NOTO3D_INFO("M2_OK   demo_checker fixture rendered with diffuse "
                                    "texture (0009 E2E)");
                    } else {
                        NOTO3D_WARN("M2_WARN  demo_checker fixture rendered WITHOUT "
                                    "a diffuse texture");
                    }
                    scene.createEntity("demo_checker", "demo_checker");  // identity transform
                    sceneName = "demo_checker";
                    resolver = [demoPtr](const std::string& id) -> const noto3d::renderer::Model* {
                        return (id == "demo_checker") ? demoPtr : nullptr;
                    };
                }

            } else if (opts.assetId) {
                // Single-entity Scene from --asset=<id>; M3 identity line kept.
                const noto3d::asset::AssetEntry* entry = registry.findEntry(*opts.assetId);
                if (!entry) {
                    const std::string msg =
                        "M2_FAIL: unknown asset '" + *opts.assetId + "'";
                    ok = fail(msg.c_str());
                } else {
                    NOTO3D_INFO("M3_INFO  selected asset '{}' geometry={} reference={}",
                                entry->id, geometrySourceName(*entry), referenceName(*entry));
                    scene.createEntity(entry->id, entry->id);  // identity transform
                    sceneName = entry->id;
                    resolver = [&registry](const std::string& id)
                                   -> const noto3d::renderer::Model* {
                        return registry.model(id);
                    };
                }

            } else {
                // Default: m4_vertical_scene — 3 entities spread on the X axis.
                const auto addAt = [&](const std::string& assetId, float x) {
                    const auto eid = scene.createEntity(assetId, assetId);
                    if (auto* e = scene.findEntity(eid)) {
                        e->transform.position = {x, 0.0f, 0.0f};
                    }
                };
                addAt("smile",       -1.5f);
                addAt("crown",        0.0f);
                addAt("sunglasses",   1.5f);
                sceneName = "m4_vertical_scene";
                resolver = [&registry](const std::string& id)
                               -> const noto3d::renderer::Model* {
                    return registry.model(id);
                };
            }

            // Structured log — 0024's automated matrix asserts on this line.
            if (ok) {
                NOTO3D_INFO("M4_INFO rendering scene '{}' entities={}",
                            sceneName,
                            static_cast<int>(scene.entities().size()));
            }

            // Window title: single-entity rule or scene name (ADR-0010/ticket 0022)
            if (ok) {
                std::string title;
                if (opts.demo) {
                    title = "demo_checker [placeholder] | noto3d (M4)";
                } else if (opts.assetId) {
                    const auto* entry = registry.findEntry(*opts.assetId);
                    const std::string src =
                        entry ? geometrySourceName(*entry) : "unknown";
                    title = *opts.assetId + " [" + src + "] | noto3d (M4)";
                } else {
                    title = sceneName + " | noto3d (M4)";
                }
                glfwSetWindowTitle(window, title.c_str());
            }

            // ---- Render loop ---------------------------------------------------
            if (ok) {
                while (!glfwWindowShouldClose(window) &&
                       (opts.frameLimit < 0 || frames < opts.frameLimit)) {
                    glfwPollEvents();
                    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                    }

                    // Compute view + projection (camera in 0023; constants held here)
                    int fbW = 0, fbH = 0;
                    glfwGetFramebufferSize(window, &fbW, &fbH);
                    const float aspect = (fbH > 0)
                        ? static_cast<float>(fbW) / static_cast<float>(fbH) : 1.0f;
                    const auto proj = glm::perspective(glm::radians(45.0f),
                                                       aspect, 0.1f, 100.0f);
                    const auto view = glm::lookAt(glm::vec3(0.0f, 0.5f, 3.0f),
                                                  glm::vec3(0.0f, 0.0f, 0.0f),
                                                  glm::vec3(0.0f, 1.0f, 0.0f));

                    renderer.Begin3DPass();
                    // All draws go through collectDrawItems — no bypass (ADR-0010 Q6)
                    for (const auto& item :
                             noto3d::app::collectDrawItems(scene, resolver)) {
                        const glm::mat4 mvp = proj * view * item.modelMatrix;
                        for (const auto& sm : *item.model) {
                            renderer.submit(sm.mesh, sm.material, baseShader, mvp);
                        }
                    }
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
    // --list-assets exits before any GL context is created (ticket 0017).
    if (opts.listAssets) {
        return listAssetsCommand();
    }
    return run(opts);
}