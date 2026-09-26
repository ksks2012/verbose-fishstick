# 0011 — M1 code-quality deferred findings

**Source:** Code review of tickets 0005/0006 (Sep 2026)
**Status:** folded into 0012 — resolved as part of the 0012 app/renderer separation
**Blocked by:** 0012

> **Disposition:** ⑦⑧⑨ are resolved inside 0012's main.cpp restructure (each is a natural
> consequence of that change); ⑩ is a renderer-layer optimization, folded into 0012's renderer
> work or handled as a separate follow-up. This ticket is not executed independently.

---

## ⑦ NOTO3D_SHADER_DIR fallback has cwd dependency

`src/app/main.cpp`:
```cpp
#ifndef NOTO3D_SHADER_DIR
#define NOTO3D_SHADER_DIR "./shaders"
#endif
```
The fallback `"./shaders"` only resolves correctly if the binary is run from the repo root.
CMake builds always define `NOTO3D_SHADER_DIR` to an absolute path at compile time, so this
never triggers in practice. Risk: low; mitigation: document the cwd assumption or replace
fallback with a build-time error if the define is absent.

**Suggested fix:** replace the `#ifndef` fallback with:
```cpp
#ifndef NOTO3D_SHADER_DIR
#error "NOTO3D_SHADER_DIR must be defined by the build system"
#endif
```

---

## ⑧ `std::optional<ShaderProgram>` wrapping is cosmetic after 0007

`src/app/main.cpp` uses `std::optional<ShaderProgram>` and `std::optional<Mesh>` because
neither type is default-constructible and both are created inside a try block.
This is correct but produces `->` syntax throughout the render loop.

**When 0007 lands:** the renderer pass will encapsulate setup; `main.cpp` will no longer
manage `ShaderProgram`/`Mesh` directly. The optional wrappers disappear naturally.

**No action needed before 0007.**

---

## ⑨ `--frames=abc` silently returns 0 (false-green)

`src/app/main.cpp`:
```cpp
frameLimit = std::atoi(argv[i] + 9);  // "abc" → 0
```
`atoi("abc")` returns 0, so `--frames=abc` runs 0 frames and exits 0 — indistinguishable
from a valid `--frames=0` call. Inherited from M0; not introduced here.

**Suggested fix:** use `std::stoi` with a try/catch, or validate that `frameLimit >= 0`
after parsing and print an error + exit 1 if the value makes no sense.

---

## ⑩ `glGetUniformLocation` called per frame

`src/renderer/shader_program.cpp` — `setMat4`, `setVec4`, `setFloat` each call
`glGetUniformLocation` at every invocation.  For M1 with one draw call this is immaterial.
When the renderer grows (0007 pass, multiple materials), caching locations in a
`std::unordered_map<std::string_view, GLint>` inside `ShaderProgram` will eliminate
repeated driver round-trips.

**Suggested fix (deferred to 0007 or later):** cache uniform locations lazily on first query
and reuse for subsequent frames.
