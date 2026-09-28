# ADR-0004 — OpenGL 3.3 Core / GLSL 330 renderer baseline

Status: Accepted

## Context
Hardware ceiling is far above 3.3 (Mesa 25.2), but nothing in MVP/MVP+1 needs 4.x
(no compute, tessellation, SSBO, texture storage). 3.3 Core is the widest compatibility floor
that covers the whole roadmap. sRGB color management must be a baseline behavior or it silently
misrepresents Noto reference colors later.

## Decision
- OpenGL 3.3 Core; GLSL `#version 330 core`; no forward-compat flag.
- GLFW hints: version 3.3, core profile, depth 24, `GLFW_SAMPLES 4`.
- Context verification is **numeric** using `GL_MAJOR_VERSION` / `GL_MINOR_VERSION` and the core-
  profile bit from `GL_CONTEXT_PROFILE_MASK`, with function pointers loaded through glad — NOT
  string prefix parsing of `glGetString(GL_VERSION)`; version/renderer strings are diagnostics only.
- sRGB at baseline now: `GLFW_SRGB_CAPABLE`, `GL_FRAMEBUFFER_SRGB` enabled, color/diffuse
  textures use `GL_SRGB8_ALPHA8` where applicable (not assumed for future non-color textures such
  as masks/lookups). Reference images stay reference-only (ADR-0002).
- MSAA 4x is part of the renderer baseline (not a later enhancement).
- Initialization order: create window+context → glad load → numeric capability validation → state.
- Debug builds only: `GLFW_OPENGL_DEBUG_CONTEXT` requests a debug context; after glad loads, the
  code verifies `GL_KHR_debug` availability before calling `glDebugMessageCallback`/`glDebugMessageControl`
  (never assume the callback exists), severity filtered (NOTIFICATION hidden).
- Draw loop: double-buffered, `glfwSwapInterval(1)`, viewport from framebuffer size, resize callback.

## Consequences
- Single color pipeline (sRGB→linear→light→linear→framebuffer) from day one.
- M1 screenshots remain consistent with M2/M3.

## Amendment — viewport is not the framebuffer (2026-09-28, ADR-0011 Q0)

The draw-loop clause above — "viewport from framebuffer size" — held for M1–M4 because the 3D view
*was* the whole framebuffer. M5 introduces a docked editor, so that coincidence ends:

- **The active GL viewport is the editor's 3D view rect**, not the framebuffer. The app layer
  establishes `glViewport` (and the matching scissor box) from an explicit `ViewportRect` before
  the 3D pass — see the ADR-0005 amendment.
- **Projection aspect comes from the viewport rect** (`width / height`), not from the full
  framebuffer size. A non-square viewport must not stretch the render.
- **DPI handling is unchanged**: the swap is in framebuffer pixels, so the sRGB/DPI-safety intent of
  the original clause is preserved. The rectangle simply is not the whole window anymore.
- `glfwGetFramebufferSize` and the resize callback still drive the framebuffer size; they now feed
  the framebuffer-space origin the `ViewportRect` is expressed in, rather than the viewport itself.

Consequence: no GL capability, sRGB, MSAA, or initialization-order decision changes. This amendment
only retires the assumption that viewport ≡ framebuffer.