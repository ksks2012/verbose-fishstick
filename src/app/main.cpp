#include <glad/gl.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <asset_picker.hpp>
#include <asset_registry.hpp>
#include <camera.hpp>
#include <camera_input.hpp>
#include <cli_options.hpp>
#include <draw_list.hpp>
#include <editor_entry.hpp>
#include <gl_capability.hpp>
#include <log.hpp>
#include <renderer.hpp>
#include <scene.hpp>
#include <scene_file_io.hpp>
#include <scene_io.hpp>
#include <shader_program.hpp>
#include <viewport_state.hpp>

#include <dock_layout.hpp>
#include <editor_shell.hpp>
#include <imgui_harness.hpp>
#include <panel_render.hpp>

#include <string>
#include <utility>
#include <vector>
#include <window_input.hpp>

#include <cstdlib>
#include <optional>

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
// All GL-owning objects (shader, registry → Mesh/Texture, renderer, ImGui
// context) are locals of renderScene() so they are destroyed while the context
// is still current — never after glfwDestroyWindow/glfwTerminate (which would
// delete GL objects without a context).  Making the function itself the scope
// is what guarantees that ordering: run() keeps only the GLFW lifecycle
// (ticket 0025).
int renderScene(GLFWwindow* window, const noto3d::app::Options& opts,
                noto3d::scene::Scene preloadedScene) {
    int exitCode = EXIT_SUCCESS;
    int frames   = 0;
    bool ranLoop = false;

    noto3d::app::InputState       inputState;
    noto3d::ui::installWindowCallbacks(window, inputState);

    noto3d::renderer::ShaderProgram baseShader;
    noto3d::asset::AssetRegistry    registry;
    noto3d::renderer::Renderer      renderer;
    noto3d::app::ViewportState      viewState;  // camera + viewport rect (ticket 0028)
    noto3d::ui::DockLayout          dockLayout;

    // ---- M1 — base shader (ticket 0011-⑧: no optional wrapper, no inline cube)
    if (!loadBaseShader(baseShader)) {
        // M1_FAIL already logged; skip straight to scope teardown.
        return EXIT_FAILURE;
    }
    NOTO3D_INFO("M1_OK base shader compiled; assets load via AssetRegistry");

    // ---- M2 — manifest assets through the AssetRegistry (ticket 0012)
    const auto loadR = registry.load(std::string(NOTO3D_ASSETS_DIR) + "/manifest.json");
    for (const auto& d : loadR.diagnostics) {
        NOTO3D_WARN("M2_WARN  [{}] {}", d.assetId.empty() ? "manifest" : d.assetId, d.message);
    }
    NOTO3D_INFO("M2_INFO  manifest loaded: {} ok, {} warned, {} failed",
                loadR.loaded, loadR.warned, loadR.failed);

    // ---- M4 — Scene construction (ADR-0010 Q5/Q6, editor_entry.cpp) --------
    auto entry = noto3d::app::buildEntryScene(opts, registry, std::move(preloadedScene));
    if (!entry) {
        return EXIT_FAILURE;  // diagnostic already logged
    }

    // ---- M6 — --save-scene= output (ADR-0013 Q7/Q8) ------------------------
    // Written ONCE, after the entry scene is built and BEFORE the frame loop, so
    // the document is exactly what the run started from.  It is an OUTPUT option,
    // not an entry mode, so it combines with --asset/--demo/--scene and with the
    // default (which is what makes "--demo --save-scene=out.scene" work).
    // Q6: no new Editor API — Editor::scene() already returns const Scene&.
    //
    // This stays AFTER the GL context for the asset-backed modes because the Scene
    // it serializes comes from the registry, and the registry's GPU cache is why
    // the context exists (ADR-0010 Q5 / 0025's lifetime rule).  A --scene= run has
    // no such dependency, but writing it early would emit M6_INFO scene-saved
    // before M4_INFO rendering scene, inverting the order the close matrix reads
    // them; one code path that is correct beats two that are correct differently.
    if (opts.saveScenePath) {
        // writeSceneFileAtomic() already logged the exact path and reason, so
        // there is nothing to add here: re-logging produced two M6_FAIL lines for
        // one failure, the second of which named only the destination and dropped
        // the cause.
        if (!noto3d::app::saveSceneTo(*opts.saveScenePath, entry->editor.scene())) {
            return EXIT_FAILURE;
        }
        NOTO3D_INFO("M6_INFO scene-saved  path='{}' entities={}", *opts.saveScenePath,
                    static_cast<int>(entry->editor.entityCount()));
    }

    // Asset-picker per-selection state (ADR-0011 Q7): a VALUE snapshot rebuilt
    // from the registry when the selection changes; emptied on deselect.
    noto3d::scene::EntityId      lastPickerSelection = noto3d::scene::InvalidEntityId;
    noto3d::app::AssetPickerSnapshot pickerSnapshot;
    glfwSetWindowTitle(window, entry->windowTitle.c_str());

    // ImGui is created here so its GL objects die while the context is current.
    // Throwing is the only failure mode; the context teardown in run() still
    // runs, and the process exits non-zero via the catch below.
    std::optional<noto3d::ui::ImGuiHarness> imgui;
    try {
        imgui.emplace(window);
    } catch (const std::exception& e) {
        NOTO3D_CRITICAL("M0_FAIL: ImGui initialisation failed — {}", e.what());
        return EXIT_FAILURE;
    }

    // ---- Render loop ---------------------------------------------------
    while (!glfwWindowShouldClose(window) &&
           (opts.frameLimit < 0 || frames < opts.frameLimit)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        imgui->newFrame();
        dockLayout.submitDockHost();

        // Asset-picker snapshot (ADR-0011 Q7): a VALUE snapshot rebuilt from
        // the registry when the picker needs it — on a selection change, never
        // per frame (Q8: no per-frame logging).  The UI freely keeps and sorts
        // this value; it never holds a span into registry storage.  Empty while
        // nothing is selected: the Inspector offers no picker then.
        {
            const auto sel = entry->editor.selectedId();
            if (entry->editor.hasSelection() && sel != lastPickerSelection) {
                lastPickerSelection = sel;
                pickerSnapshot = noto3d::app::buildAssetPickerSnapshot(registry);
            } else if (!entry->editor.hasSelection() && sel != lastPickerSelection) {
                lastPickerSelection = sel;
                pickerSnapshot = {};
            }
        }

        // Panels report the 3D panel's extent and the ownership inputs; the
        // panel submission order inside is itself a contract (ticket 0034 ②).
        const noto3d::ui::ShellFrame shell =
            noto3d::ui::submitEditorPanels(window, entry->editor, pickerSnapshot);
        viewState.rect = shell.rect;

        noto3d::ui::applyCameraInputForFrame(window, inputState, shell, viewState.camera);

        // Scissored + viewported to the panel rect; clears only inside it.
        noto3d::ui::renderPanel3D(window, viewState, entry->editor.scene(), entry->resolve,
                                  renderer, baseShader);

        imgui->render();

        glfwSwapBuffers(window);
        ++frames;
    }
    ranLoop = true;

    if (ranLoop) NOTO3D_INFO("M1_DONE rendered {} frames", frames);
    // Every GL object above is destroyed on return — while the context is current.
    return exitCode;
}

}  // namespace

static int run(const noto3d::app::Options& opts, noto3d::scene::Scene preloadedScene) {
    if (!glfwInit()) {
        NOTO3D_CRITICAL("M0_FAIL: glfwInit failed");
        return EXIT_FAILURE;
    }
    noto3d::ui::installGlfwErrorCallback();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
#if NOTO3D_DEBUG_GL
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    GLFWwindow* window =
        glfwCreateWindow(kInitialW, kInitialH, kWindowTitle, nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        NOTO3D_CRITICAL("M0_FAIL: window/context creation failed");
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

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

    // Debug GL context setup — one-time, not a per-pass concern (ADR-0004).
    //
    // The return value is deliberately discarded: the callee already self-reports
    // ("M0_WARN debug context requested but GL_KHR_debug unavailable"). Note that in a
    // release build NOTO3D_DEBUG_GL is 0, so the whole body compiles out and `false`
    // means "debug GL was never requested" — not a failure. Warning here would emit a
    // spurious M0_WARN on every release run.
    static_cast<void>(noto3d::ui::installGlDebugCallback());

    // The whole render path (GL objects, Scene, main loop) lives in renderScene()
    // so every GL object dies before the context does.
    const int exitCode = renderScene(window, opts, preloadedScene);

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
        return noto3d::app::runListAssetsCommand();
    }
    // The entry modes are mutually exclusive (ADR-0013 Q5): --asset, --demo and
    // --scene.  Rejected pre-GL so the regression matrices can verify exit 1 with
    // no display.
    //
    // ONE table drives both the count and the message.  An earlier version spelled
    // the count out as a sum of three conditionals and then repeated the same three
    // flags in a second block to build the message; that is two copies of the mode
    // set, and adding a fourth mode would have had to be remembered twice -- with
    // nothing catching a miss, so the new mode would silently vanish from the
    // diagnostic while still being counted.
    const std::vector<std::pair<const char*, bool>> entryModes{
        {"--asset", opts.assetId.has_value()},
        {"--demo", opts.demo},
        {"--scene", opts.scenePath.has_value()},
    };
    int         modeCount = 0;
    std::string given;
    for (const auto& [flag, active] : entryModes) {
        if (!active) continue;
        if (modeCount > 0) given += " and ";
        given += flag;
        ++modeCount;
    }
    if (modeCount > 1) {
        NOTO3D_CRITICAL("M6_FAIL: {} are mutually exclusive entry modes", given);
        return EXIT_FAILURE;
    }

    // ---- M6 — --scene= deserialized PRE-GL (0046) ---------------------------
    // A malformed .scene is a user error, not a rendering error, so it must not
    // cost a window or a context.  Ticket 0046 requires every structural-failure
    // row of the close matrix to exit non-zero with NO display, which is only
    // true if the parse happens here rather than inside renderScene() — and the
    // parse genuinely needs nothing GL: readSceneFile + deserializeScene touch the
    // filesystem and nlohmann only, which is the 0041 arch guard's whole point.
    //
    // This does NOT mean a valid document skips the registry: buildEntryScene
    // still installs the Scene and the resolver, so an unresolvable assetId is
    // still warned and skipped at render.  Only the FAILURE moved earlier.
    noto3d::scene::Scene preloadedScene;
    if (opts.scenePath) {
        noto3d::serialization::LoadResult loaded =
            noto3d::app::loadSceneFrom(*opts.scenePath);
        if (!loaded.scene) {
            // loadSceneFrom already re-emitted 0044's diagnostic taxonomy.
            return EXIT_FAILURE;
        }
        NOTO3D_INFO("M6_INFO scene-loaded path='{}' entities={}", *opts.scenePath,
                    static_cast<int>(loaded.scene->entities().size()));
        preloadedScene = std::move(*loaded.scene);
    }
    return run(opts, std::move(preloadedScene));
}
