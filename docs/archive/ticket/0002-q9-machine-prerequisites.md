# 0002 — Q9 machine prerequisites (GLFW X11)

**What to build:**

Environment gate: install the one-time system packages required to build the fetched GLFW X11
source (`libxinerama-dev`, `libxcursor-dev`, `libxi-dev` — the verified Q9 set). This is the Q9
exception to the "no sudo" contract (ADR-0003): unlike project-owned dependencies, these machine
prerequisites are allowed to require a one-time `sudo apt install`.

Before this ticket, a fresh `cmake --preset dev` configure aborts at GLFW's internal X11 probe
("Xinerama headers not found"). After it, configure is able to proceed past that point.

Execution requires the user (or a privileged agent) to run the system install manually;
the acceptance check below validates the unblocked configure.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [x] `libxinerama-dev`, `libxcursor-dev`, and `libxi-dev` are installed system-wide
- [x] Re-running `cmake --preset dev` no longer aborts with the missing-Xinerama-headers error
      (the Q9 gate is cleared)
- [x] The configure step reaches the GLFW X11 probe stage successfully (X11 found, no header abort)