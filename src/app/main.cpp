#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <asset_registry.hpp>
#include <camera.hpp>
#include <cli_options.hpp>
#include <draw_list.hpp>
#include <gl_capability.hpp>
#include <input_gate.hpp>
#include <log.hpp>
#include <renderer.hpp>
#include <scene.hpp>
#include <viewport_rect.hpp>
#include <viewport_state.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
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

// Window geometry/title stay here (GLFW concerns, not CLI syntax) — ticket 0025.
const char* kWindowTitle = "noto3d (M4)";
const int   kInitialW    = 1280;
const int   kInitialH    = 720;

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

// Camera interaction sensitivities (app layer: pixels → world units / radians)
// Unit conversion is here, never inside Camera (ADR-0010 Q7 / ticket 0023).
constexpr float kOrbitSens = 0.005f;  // radians/pixel
constexpr float kPanSens   = 0.003f;  // world-units/pixel
constexpr float kZoomSens  = 0.5f;    // world-units/scroll-tick

// Per-frame mouse state for camera interaction (no InputManager at M4 — ADR-0010)
struct InputState {
    double prevX     = 0.0;
    double prevY     = 0.0;
    float  scrollY   = 0.0f;
    bool   firstMove = true;
};

void scrollCallback(GLFWwindow* w, double /*xoffset*/, double yoffset) {
    auto* s = static_cast<InputState*>(glfwGetWindowUserPointer(w));
    if (s) s->scrollY += static_cast<float>(yoffset);
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

// ---- Scene scope --------------------------------------------------------
// All GL-owning objects (shader, registry → Mesh/Texture, renderer) are locals
// of renderScene() so they are destroyed while the context is still current —
// never after glfwDestroyWindow/glfwTerminate (which would delete GL objects
// without a context).  Making the function itself the scope is what guarantees
// that ordering: run() keeps only the GLFW lifecycle (ticket 0025).
int renderScene(GLFWwindow* window, const noto3d::app::Options& opts) {
    int  exitCode = EXIT_SUCCESS;
    int  frames   = 0;
    bool ranLoop  = false;
    InputState inputState;
    glfwSetWindowUserPointer(window, &inputState);

    noto3d::renderer::ShaderProgram baseShader;
    noto3d::asset::AssetRegistry    registry;
    noto3d::renderer::Renderer      renderer;
    noto3d::app::ViewportState      viewState;   // camera + viewport rect (ticket 0028)

    // ImGui initialisation (M5 docking shell) — RAII teardown on return.
    // Created inside the GL scope so all ImGui GL objects are valid.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename  = nullptr;  // layout set programmatically; no ini file
    }
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    struct ImGuiGuard {
        ~ImGuiGuard() {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
    } imguiGuard;

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

        if (opts.demo) {
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
            bool firstLayout = true;

            while (!glfwWindowShouldClose(window) &&
                   (opts.frameLimit < 0 || frames < opts.frameLimit)) {
                glfwPollEvents();
                if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }

                // ── ImGui new frame ──────────────────────────────────────────
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                // ── Dock host (invisible full-window container) ──────────────
                {
                    const ImGuiViewport* vp = ImGui::GetMainViewport();
                    ImGui::SetNextWindowPos(vp->Pos);
                    ImGui::SetNextWindowSize(vp->Size);
                    ImGui::SetNextWindowViewport(vp->ID);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                    const ImGuiWindowFlags hostFlags =
                        ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_NoTitleBar   |
                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize     |
                        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBackground |
                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                        ImGuiWindowFlags_NoNavFocus;
                    ImGui::Begin("##DockHost", nullptr, hostFlags);
                    ImGui::PopStyleVar(3);
                    const ImGuiID dsID =
                        ImGui::GetID("MainDockspace");
                    ImGui::DockSpace(dsID, ImVec2(0, 0));

                    // First-frame default layout (programmatic; no .ini) ──────
                    if (firstLayout) {
                        ImGui::DockBuilderRemoveNode(dsID);
                        ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);
                        ImGui::DockBuilderSetNodeSize(dsID, vp->Size);

                        ImGuiID left, right;
                        ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Left, 0.20f, &left, &right);
                        ImGuiID inspector, view;
                        ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.25f, &inspector, &view);

                        ImGui::DockBuilderDockWindow("Hierarchy", left);
                        ImGui::DockBuilderDockWindow("3D View",   view);
                        ImGui::DockBuilderDockWindow("Inspector", inspector);
                        ImGui::DockBuilderFinish(dsID);
                        firstLayout = false;
                    }
                    ImGui::End();
                }

                // ── Hierarchy panel (empty placeholder) ──────────────────────
                ImGui::Begin("Hierarchy");
                ImGui::End();

                // ── Inspector panel (empty placeholder) ──────────────────────
                ImGui::Begin("Inspector");
                ImGui::End();

                // ── Input gate state (queried before the 3D panel adds items) ─
                // These reflect all items in Hierarchy + Inspector that were
                // built above; the 3D-view Dummy() below does NOT affect them.
                const bool anyItemHovered    = ImGui::IsAnyItemHovered();
                const bool anyItemActive     = ImGui::IsAnyItemActive();
                const bool globalModalCapture =
                    ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId |
                                           ImGuiPopupFlags_AnyPopupLevel);

                // ── 3D View panel ─────────────────────────────────────────────
                // Transparent bg so the GL rendering is visible through the panel.
                // NoScrollbar + NoScrollWithMouse: wheel is owned by the camera gate.
                noto3d::app::ViewportRect vpr;  // will be populated below
                bool viewportHovered = false;
                {
                    ImGui::SetNextWindowBgAlpha(0.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    const ImGuiWindowFlags viewFlags =
                        ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse;
                    ImGui::Begin("3D View", nullptr, viewFlags);
                    viewportHovered =
                        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

                    // Content region → GL-space rect (Y-flip: ImGui origin top-left)
                    const ImVec2 cpos  = ImGui::GetCursorScreenPos();
                    const ImVec2 csize = ImGui::GetContentRegionAvail();
                    int fbW, fbH;
                    glfwGetFramebufferSize(window, &fbW, &fbH);
                    vpr = noto3d::app::ViewportRect{
                        static_cast<int>(cpos.x),
                        // Y-flip (ADR-0011 Q0): gl_y = fbH - (imgui_top + height)
                        static_cast<int>(fbH - (cpos.y + csize.y)),
                        static_cast<int>(csize.x),
                        static_cast<int>(csize.y)
                    };
                    // Advance cursor over the content area (does not register hover).
                    ImGui::Dummy(csize);
                    ImGui::End();
                    ImGui::PopStyleVar();
                }
                // Store the current rect back into the ViewportState.
                viewState.rect = vpr;

                // ── Camera input (gated; denied scroll discarded) ────────────
                {
                    const bool gateOpen =
                        noto3d::app::viewportGateOpen(viewportHovered,
                                                       anyItemHovered,
                                                       anyItemActive,
                                                       globalModalCapture);

                    double curX, curY;
                    glfwGetCursorPos(window, &curX, &curY);
                    if (inputState.firstMove) {
                        inputState.prevX     = curX;
                        inputState.prevY     = curY;
                        inputState.firstMove = false;
                    }
                    const float dx = static_cast<float>(curX - inputState.prevX);
                    const float dy = static_cast<float>(curY - inputState.prevY);
                    inputState.prevX = curX;
                    inputState.prevY = curY;

                    if (gateOpen) {
                        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                            viewState.camera.orbit(dx * kOrbitSens, -dy * kOrbitSens);
                        }
                        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
                            viewState.camera.pan(dx * kPanSens, -dy * kPanSens);
                        }
                    }
                    // Scroll: always discard; zoom only when gate open (ADR-0011 Q10)
                    if (inputState.scrollY != 0.0f) {
                        if (gateOpen) {
                            viewState.camera.zoom(inputState.scrollY * kZoomSens);
                        }
                        inputState.scrollY = 0.0f;  // discard even if gate closed
                    }
                }

                // ── 3D render — scissored and viewported to the panel rect ────
                if (vpr.valid()) {
                    // Scissor confines both the draw calls and the Begin3DPass
                    // clear to the 3D panel; regions outside are left to ImGui.
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(vpr.x, vpr.y, vpr.width, vpr.height);
                    glViewport(vpr.x, vpr.y, vpr.width, vpr.height);

                    const auto proj = viewState.camera.projectionMatrix(vpr.aspect());
                    const auto view = viewState.camera.viewMatrix();

                    renderer.Begin3DPass();  // clears within scissor, sets 3D state
                    for (const auto& item :
                             noto3d::app::collectDrawItems(scene, resolver)) {
                        const glm::mat4 mvp = proj * view * item.modelMatrix;
                        for (const auto& sm : *item.model) {
                            renderer.submit(sm.mesh, sm.material, baseShader, mvp);
                        }
                    }
                    renderer.End3DPass();

                    glDisable(GL_SCISSOR_TEST);
                    // Restore full-framebuffer viewport for ImGui rendering.
                    int fbW, fbH;
                    glfwGetFramebufferSize(window, &fbW, &fbH);
                    glViewport(0, 0, fbW, fbH);
                }

                // ── ImGui render (draws panels on top of the 3D content) ──────
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                glfwSwapBuffers(window);
                ++frames;
            }
            ranLoop = true;
        }
    }

    if (ranLoop) NOTO3D_INFO("M1_DONE rendered {} frames", frames);
    // Every GL object above is destroyed on return — while the context is current.
    return exitCode;
}

}  // namespace

static int run(const noto3d::app::Options& opts) {
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
    glfwSetScrollCallback(window, scrollCallback);  // camera zoom (ticket 0023)

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

    // The whole render path (GL objects, Scene, main loop) lives in renderScene()
    // so every GL object dies before the context does.
    const int exitCode = renderScene(window, opts);

    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}

int main(int argc, char** argv) {
    noto3d::core::log_init();

    noto3d::app::Options opts;
    if (!noto3d::app::parseOptions(argc, argv, opts)) {
        return EXIT_FAILURE;
    }
    // --list-assets exits before any GL context is created (ticket 0017).
    if (opts.listAssets) {
        return listAssetsCommand();
    }
    // --asset and --demo are mutually exclusive; reject pre-GL so the
    // regression test (ticket 0024) can verify exit 1 without a display.
    if (opts.assetId && opts.demo) {
        NOTO3D_CRITICAL("M4_FAIL: --asset and --demo are mutually exclusive");
        return EXIT_FAILURE;
    }
    return run(opts);
}