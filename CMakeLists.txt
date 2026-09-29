cmake_minimum_required(VERSION 3.28)

project(noto3d_editor
    VERSION 0.1.0
    DESCRIPTION "C++/OpenGL editor for Noto 3D-style emoji composition (MVP)"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(NOTO3D_WERROR     "Treat warnings as errors" OFF)
option(NOTO3D_SANITIZERS "Enable ASan+UBSan in this build" OFF)
option(NOTO3D_DEBUG_GL   "Request a debug OpenGL context (via GLFW hint)" OFF)

# ---------------------------------------------------------------------------
# Warnings (own code only — always on per ADR-0008; never applied to third-party targets)
# ---------------------------------------------------------------------------
function(noto3d_set_warnings TARGET)
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic)
    if(NOTO3D_WERROR)
        target_compile_options(${TARGET} PRIVATE -Werror)
    endif()
endfunction()

function(noto3d_apply_sanitizers TARGET)
    if(NOTO3D_SANITIZERS)
        target_compile_options(${TARGET} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${TARGET} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Dependencies (ADR-0003) — layout mirrors ../reimagined-meme:
# each external library lives in ${CMAKE_SOURCE_DIR}/lib/<name>, populated by
# FetchContent with SOURCE_DIR at configure time. lib/ is gitignored except the
# two deps that cannot be fetched and are committed:
#   lib/glad — generated GL 3.3 Core loader + GL_KHR_debug
#   lib/stb  — stb_image single header
# Order matters: GLFW runs its internal Xinerama X11 probe during configure and
# aborts until libxinerama-dev/libxcursor-dev are installed (Q9 machine prereq),
# so it is configured last to keep the other lib/ clones flowing.
# SOURCE_DIR points to a pre-populated lib/ directory so disable git-update
# checks (prevents FetchContent from trying to run git in a non-git directory).
# ---------------------------------------------------------------------------
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
include(FetchContent)

set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)

# nlohmann/json — header-only
set(NLOHMANN_JSON_DIR ${LIB_DIR}/nlohmann_json)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03  # v3.11.3
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${NLOHMANN_JSON_DIR})

# GLM
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(GLM_DIR ${LIB_DIR}/glm)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0af55ccecd98d4e5a8d1fad7de25ba429d60e863  # 1.0.1
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${GLM_DIR})

# ImGui — docking required (M5); NOT merged into plain master
set(IMGUI_DIR ${LIB_DIR}/imgui)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        9b4eb24cee2071e61dc1f9ef3e5228097cdde720  # v1.92.9-docking
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${IMGUI_DIR})

# tinyobjloader — OBJ/MTL parser (ADR-0003, deferred to M2)
set(TINYOBJ_DIR ${LIB_DIR}/tinyobjloader)
FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        e60d33385e2e4f7fa891513150f2532b5bbcb093  # v1.0.6
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${TINYOBJ_DIR})

# spdlog — structured logging backend for noto3d::core::Log (ADR-0008)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(SPDLOG_DIR ${LIB_DIR}/spdlog)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        27cb4c76708608465c413f6d0e6b8d99a4d84302  # v1.14.1
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${SPDLOG_DIR})

# GLFW — X11 backend (configure last, see comment above)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# Wayland backend needs xkbcommon via pkg-config; ADR-0003 pins the X11 backend, whose
# deps (Xinerama/Xcursor/XInput/XFixes headers) are found via find_path/find_library.
# Keep pkg-config out of the build contract.
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_DIR ${LIB_DIR}/glfw)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        7b6aead9fb88b3623e3b3725ebb42670cbe4c579  # 3.4
    GIT_SHALLOW    TRUE
    SOURCE_DIR     ${GLFW_DIR})

# ── Q9 Machine-prerequisite check (user story 4) ──────────────────────────
# GLFW 3.4 (X11 backend) requires these headers at configure time.  Give a
# clear, actionable error before FetchContent_MakeAvailable triggers GLFW's
# own CMake configure and emits cryptic assertion failures.
if(UNIX AND NOT APPLE)
    find_path(XINERAMA_H_PATH "X11/extensions/Xinerama.h")
    find_path(XCURSOR_H_PATH  "X11/Xcursor/Xcursor.h")
    if(NOT XINERAMA_H_PATH OR NOT XCURSOR_H_PATH)
        message(FATAL_ERROR
            "[noto3d] Missing X11 development headers required by GLFW 3.4 (X11 backend).\n"
            "  Install them once on the build machine:\n"
            "    sudo apt install libxinerama-dev libxcursor-dev libxi-dev libxrandr-dev\n"
            "  Then re-run: cmake --preset dev")
    endif()
endif()

FetchContent_MakeAvailable(nlohmann_json glm imgui tinyobjloader spdlog glfw)

# Suppress warnings from tinyobjloader headers when included by our code
if(TARGET tinyobjloader)
    set_target_properties(tinyobjloader PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
            "$<TARGET_PROPERTY:tinyobjloader,INTERFACE_INCLUDE_DIRECTORIES>")
endif()

# Suppress compiler warnings from spdlog's own headers when included by our code
if(TARGET spdlog)
    set_target_properties(spdlog PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
            "$<TARGET_PROPERTY:spdlog,INTERFACE_INCLUDE_DIRECTORIES>")
endif()

# ---------------------------------------------------------------------------
# Vendored libraries (ADR-0003) — committed under lib/
# ---------------------------------------------------------------------------
add_library(glad STATIC ${LIB_DIR}/glad/src/gl.c)
target_include_directories(glad PUBLIC ${LIB_DIR}/glad/include)

add_library(stb_header INTERFACE)
target_include_directories(stb_header INTERFACE ${LIB_DIR}/stb)

# ImGui core + GLFW/OpenGL3 backends (M5; built now to validate fetch/source)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends ${LIB_DIR}/glad/include)
target_link_libraries(imgui PUBLIC glfw glad)
target_compile_definitions(imgui PUBLIC GLFW_INCLUDE_NONE IMGUI_IMPL_OPENGL_LOADER_GLAD)
# noto3d_set_warnings / noto3d_apply_sanitizers are NOT applied to third-party targets

# ---------------------------------------------------------------------------
# Core library (pure code, ADR-0008) + dependency-free tests (ADR-0008 / M0)
# ---------------------------------------------------------------------------
add_library(noto3d_core STATIC
    src/core/gl_capability.cpp
    src/core/log.cpp)
target_include_directories(noto3d_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src/core ${LIB_DIR}/glad/include)
target_link_libraries(noto3d_core PUBLIC spdlog::spdlog)
noto3d_set_warnings(noto3d_core)
noto3d_apply_sanitizers(noto3d_core)

enable_testing()
add_executable(noto3d-core-tests tests/core/gl_capability_test.cpp)
target_link_libraries(noto3d-core-tests PRIVATE noto3d_core)
noto3d_set_warnings(noto3d-core-tests)
noto3d_apply_sanitizers(noto3d-core-tests)
add_test(NAME core-gl-capability COMMAND noto3d-core-tests)

# ---------------------------------------------------------------------------
# Asset library (ADR-0007)
# ---------------------------------------------------------------------------
add_library(noto3d_asset STATIC
    src/asset/asset_registry.cpp
    src/asset/obj_loader.cpp)
target_include_directories(noto3d_asset PUBLIC
    ${CMAKE_SOURCE_DIR}/src/asset)
target_link_libraries(noto3d_asset
    PUBLIC  noto3d_renderer nlohmann_json::nlohmann_json
    PRIVATE tinyobjloader)
noto3d_set_warnings(noto3d_asset)
noto3d_apply_sanitizers(noto3d_asset)

add_executable(noto3d-registry-tests tests/asset/registry_test.cpp)
target_link_libraries(noto3d-registry-tests PRIVATE noto3d_asset)
target_compile_definitions(noto3d-registry-tests PRIVATE
    NOTO3D_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
noto3d_set_warnings(noto3d-registry-tests)
noto3d_apply_sanitizers(noto3d-registry-tests)
add_test(NAME asset-registry-contract COMMAND noto3d-registry-tests)

# Ticket 0016 — M3 Noto asset set verification & freeze: direct manifest-contract
# check (parse JSON + resolve paths + SHA-256), independent of AssetRegistry.
add_executable(noto3d-manifest-tests tests/asset/manifest_integrity_test.cpp)
target_link_libraries(noto3d-manifest-tests PRIVATE noto3d_core nlohmann_json::nlohmann_json)
target_compile_definitions(noto3d-manifest-tests PRIVATE
    NOTO3D_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
noto3d_set_warnings(noto3d-manifest-tests)
noto3d_apply_sanitizers(noto3d-manifest-tests)
add_test(NAME asset-manifest-integrity COMMAND noto3d-manifest-tests)

# ---------------------------------------------------------------------------
# Renderer library (ADR-0006)
# ---------------------------------------------------------------------------
add_library(noto3d_renderer STATIC
    src/renderer/camera.cpp
    src/renderer/material.cpp
    src/renderer/mesh.cpp
    src/renderer/renderer.cpp
    src/renderer/shader_program.cpp
    src/renderer/texture.cpp)
target_include_directories(noto3d_renderer PUBLIC
    ${CMAKE_SOURCE_DIR}/src/renderer)
target_link_libraries(noto3d_renderer
    PUBLIC  noto3d_core glm::glm glad
    PRIVATE stb_header)
noto3d_set_warnings(noto3d_renderer)
noto3d_apply_sanitizers(noto3d_renderer)

add_executable(noto3d-renderer-tests tests/renderer/mesh_test.cpp)
target_link_libraries(noto3d-renderer-tests PRIVATE noto3d_renderer)
noto3d_set_warnings(noto3d-renderer-tests)
noto3d_apply_sanitizers(noto3d-renderer-tests)
add_test(NAME renderer-mesh-contract COMMAND noto3d-renderer-tests)

add_executable(noto3d-material-tests tests/renderer/material_test.cpp)
target_link_libraries(noto3d-material-tests PRIVATE noto3d_renderer)
noto3d_set_warnings(noto3d-material-tests)
noto3d_apply_sanitizers(noto3d-material-tests)
add_test(NAME renderer-material-contract COMMAND noto3d-material-tests)

add_executable(noto3d-camera-tests tests/renderer/camera_test.cpp)
target_link_libraries(noto3d-camera-tests PRIVATE noto3d_renderer)
noto3d_set_warnings(noto3d-camera-tests)
noto3d_apply_sanitizers(noto3d-camera-tests)
add_test(NAME renderer-camera-math COMMAND noto3d-camera-tests)

# ---------------------------------------------------------------------------
# Scene library (ADR-0010 Q1–Q4) — pure data, glm only; never links asset/renderer
# ---------------------------------------------------------------------------
add_library(noto3d_scene STATIC
    src/scene/scene.cpp)
target_include_directories(noto3d_scene PUBLIC
    ${CMAKE_SOURCE_DIR}/src/scene)
target_link_libraries(noto3d_scene
    PUBLIC  glm::glm)
noto3d_set_warnings(noto3d_scene)
noto3d_apply_sanitizers(noto3d_scene)

add_executable(noto3d-scene-tests tests/scene/scene_test.cpp)
target_link_libraries(noto3d-scene-tests PRIVATE noto3d_scene noto3d_core)
noto3d_set_warnings(noto3d-scene-tests)
noto3d_apply_sanitizers(noto3d-scene-tests)
add_test(NAME scene-entity-contract COMMAND noto3d-scene-tests)

# ---------------------------------------------------------------------------
# Application library (ADR-0008/0010): app-layer orchestration, no GL state
# ---------------------------------------------------------------------------
# Declaration-only headers: every implementation lives in a .cpp (project rule).
add_library(noto3d_app STATIC
    src/app/asset_picker.cpp
    src/app/camera_input.cpp
    src/app/cli_options.cpp
    src/app/draw_list.cpp
    src/app/editor.cpp
    src/app/entity_edit.cpp
    src/app/input_gate.cpp
    src/app/transform_edit.cpp
    src/app/viewport_rect.cpp)
target_include_directories(noto3d_app PUBLIC
    ${CMAKE_SOURCE_DIR}/src/app)
# noto3d_asset is linked by ticket 0031 (the sanctioned edge promotion: the
# M5 asset picker's snapshot is built by the app layer from the registry).
# Still no ImGui/GLFW (ADR-0011 Q5): noto3d_asset is GLFW-free, headless-testable.
target_link_libraries(noto3d_app
    PUBLIC  noto3d_scene noto3d_renderer noto3d_asset)
noto3d_set_warnings(noto3d_app)
noto3d_apply_sanitizers(noto3d_app)

# Architecture guard — enforced at configure time (ADR-0011 Q5).
# noto3d_app must never link ImGui or GLFW; the app layer is headless by contract.
# The test binaries linking noto3d_app (editor-tests, transform-edit-tests, etc.)
# run without a DISPLAY, which serves as the empirical runtime confirmation.
get_target_property(_noto3d_app_link_libs noto3d_app LINK_LIBRARIES)
foreach(_lib IN LISTS _noto3d_app_link_libs)
    if("${_lib}" STREQUAL "imgui"  OR "${_lib}" STREQUAL "glfw"  OR
       "${_lib}" STREQUAL "GLFW"   OR "${_lib}" STREQUAL "glfw3" OR
       "${_lib}" STREQUAL "OpenGL::GL")
        message(FATAL_ERROR
            "[arch-guard] noto3d_app links '${_lib}' — the app layer must link "
            "neither ImGui nor GLFW (ADR-0011 Q5).  Move the dependency to "
            "noto3d_ui or the executable instead.")
    endif()
endforeach()
unset(_noto3d_app_link_libs)
unset(_lib)

add_executable(noto3d-app-tests tests/app/draw_list_test.cpp)
target_link_libraries(noto3d-app-tests PRIVATE noto3d_app noto3d_core)
noto3d_set_warnings(noto3d-app-tests)
noto3d_apply_sanitizers(noto3d-app-tests)
add_test(NAME app-draw-list-seam COMMAND noto3d-app-tests)

add_executable(noto3d-editor-tests tests/app/editor_test.cpp)
target_link_libraries(noto3d-editor-tests PRIVATE noto3d_app noto3d_core)
noto3d_set_warnings(noto3d-editor-tests)
noto3d_apply_sanitizers(noto3d-editor-tests)
add_test(NAME app-editor-facade COMMAND noto3d-editor-tests)

add_executable(noto3d-transform-edit-tests tests/app/transform_edit_test.cpp)
target_link_libraries(noto3d-transform-edit-tests PRIVATE noto3d_app noto3d_core)
noto3d_set_warnings(noto3d-transform-edit-tests)
noto3d_apply_sanitizers(noto3d-transform-edit-tests)
add_test(NAME app-transform-edit COMMAND noto3d-transform-edit-tests)

add_executable(noto3d-asset-picker-tests tests/app/asset_picker_test.cpp)
target_link_libraries(noto3d-asset-picker-tests PRIVATE noto3d_app noto3d_core)
noto3d_set_warnings(noto3d-asset-picker-tests)
noto3d_apply_sanitizers(noto3d-asset-picker-tests)
add_test(NAME app-asset-picker COMMAND noto3d-asset-picker-tests)

add_executable(noto3d-entity-edit-tests tests/app/entity_edit_test.cpp)
target_link_libraries(noto3d-entity-edit-tests PRIVATE noto3d_app noto3d_core)
noto3d_set_warnings(noto3d-entity-edit-tests)
noto3d_apply_sanitizers(noto3d-entity-edit-tests)
add_test(NAME app-entity-edit COMMAND noto3d-entity-edit-tests)

add_executable(noto3d-viewport-rect-tests
    tests/app/viewport_rect_test.cpp
    src/app/viewport_rect.cpp)
target_include_directories(noto3d-viewport-rect-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(noto3d-viewport-rect-tests PRIVATE noto3d_core)
noto3d_set_warnings(noto3d-viewport-rect-tests)
noto3d_apply_sanitizers(noto3d-viewport-rect-tests)
add_test(NAME app-viewport-rect-math COMMAND noto3d-viewport-rect-tests)

add_executable(noto3d-input-gate-tests
    tests/app/input_gate_test.cpp
    src/app/input_gate.cpp)
target_include_directories(noto3d-input-gate-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(noto3d-input-gate-tests PRIVATE noto3d_core)
noto3d_set_warnings(noto3d-input-gate-tests)
noto3d_apply_sanitizers(noto3d-input-gate-tests)
add_test(NAME app-input-gate-contract COMMAND noto3d-input-gate-tests)

# Ticket 0025 — CLI contract verified in-process (Options content, not just
# exit codes); the binary-level CLI matrix below stays as the end-to-end check.
# Parsing is a pure syntax contract (0025 ①): link noto3d_core only and compile the
# TU directly, so this suite needs no GL — it must not reach glad through
# noto3d_app's renderer::Model type dependency. The built binary stays covered
# end-to-end by the cli-* CTest matrix below.
add_executable(noto3d-cli-options-tests
    tests/app/cli_options_test.cpp
    src/app/cli_options.cpp)
target_include_directories(noto3d-cli-options-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(noto3d-cli-options-tests PRIVATE noto3d_core)
noto3d_set_warnings(noto3d-cli-options-tests)
noto3d_apply_sanitizers(noto3d-cli-options-tests)
add_test(NAME app-cli-parse-contract COMMAND noto3d-cli-options-tests)

# ---------------------------------------------------------------------------
# UI adaptation layer (ADR-0008 reserves src/ui; ADR-0011: presentation and
# input adaptation only — no editor or scene logic lives here)
# ---------------------------------------------------------------------------
add_library(noto3d_ui STATIC
    src/ui/dock_layout.cpp
    src/ui/editor_shell.cpp
    src/ui/imgui_harness.cpp
    src/ui/panel_render.cpp
    src/ui/window_input.cpp)
target_include_directories(noto3d_ui
    PUBLIC  ${CMAKE_SOURCE_DIR}/src/ui
    PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(noto3d_ui
    PUBLIC  noto3d_app
    PRIVATE glfw glad glm::glm imgui)
target_compile_definitions(noto3d_ui PRIVATE
    NOTO3D_DEBUG_GL=$<BOOL:${NOTO3D_DEBUG_GL}>)
noto3d_set_warnings(noto3d_ui)
noto3d_apply_sanitizers(noto3d_ui)

# ---------------------------------------------------------------------------
# Application
#
# editor_entry.cpp lives in src/app/ but is built into the executable, not into
# noto3d_app: it needs noto3d_asset, and promoting that edge is a target-graph
# change owned by tickets 0027 / 0031.
# ---------------------------------------------------------------------------
add_executable(noto3d-editor
    src/app/main.cpp
    src/app/editor_entry.cpp)
target_link_libraries(noto3d-editor PRIVATE
    glfw glad glm::glm imgui noto3d_ui noto3d_app noto3d_asset)
target_compile_definitions(noto3d-editor PRIVATE
    NOTO3D_DEBUG_GL=$<BOOL:${NOTO3D_DEBUG_GL}>
    NOTO3D_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders"
    NOTO3D_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
noto3d_set_warnings(noto3d-editor)
noto3d_apply_sanitizers(noto3d-editor)

# ---------------------------------------------------------------------------
# CLI regression matrix — headless-safe (ticket 0024 / ADR-0010 Q8)
#
# Only paths that exit BEFORE creating a GL context are registered here;
# they require no DISPLAY and run identically in every CI environment.
#
# GL-requiring paths (default / --asset=<id> / --demo with valid id) are
# verified manually per ADR-0008 and the 0021 headless draw-list suite covers
# the asset-resolution contract.
# ---------------------------------------------------------------------------

# --list-assets exits before GLFW; verifies the CPU-only manifest path.
add_test(NAME cli-list-assets
    COMMAND $<TARGET_FILE:noto3d-editor> --list-assets)
set_tests_properties(cli-list-assets PROPERTIES
    PASS_REGULAR_EXPRESSION "M3_ASSETS version=1")

# Unknown flag rejected in parseOptions (pre-GL) → exit 1.
add_test(NAME cli-invalid-flag
    COMMAND $<TARGET_FILE:noto3d-editor> --no-such-flag)
set_tests_properties(cli-invalid-flag PROPERTIES WILL_FAIL TRUE)

# --frames= with a negative value → exit 1.
add_test(NAME cli-frames-negative
    COMMAND $<TARGET_FILE:noto3d-editor> --frames=-1)
set_tests_properties(cli-frames-negative PROPERTIES WILL_FAIL TRUE)

# --frames= with a non-numeric value → exit 1.
add_test(NAME cli-frames-non-numeric
    COMMAND $<TARGET_FILE:noto3d-editor> --frames=abc)
set_tests_properties(cli-frames-non-numeric PROPERTIES WILL_FAIL TRUE)

# --asset= with empty id → exit 1 in parseOptions.
add_test(NAME cli-asset-empty
    COMMAND $<TARGET_FILE:noto3d-editor> "--asset=")
set_tests_properties(cli-asset-empty PROPERTIES WILL_FAIL TRUE)

# --asset + --demo mutually exclusive; check moved to main() before run() →
# exit 1 without touching GLFW/GL (ticket 0024).
add_test(NAME cli-asset-demo-exclusive
    COMMAND $<TARGET_FILE:noto3d-editor> --asset=smile --demo)
set_tests_properties(cli-asset-demo-exclusive PROPERTIES WILL_FAIL TRUE)