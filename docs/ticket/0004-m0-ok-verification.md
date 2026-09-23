# 0004 — Non-interactive M0_OK verification + capability tests

**What to build:**

Runtime gate: verify the bootstrap against the OpenGL 3.3 Core baseline and make the capability
check testable. The capability validation currently lives inline in the bootstrap; this ticket
extracts it into a dependency-free pure function `supportsRequiredContext(major, minor, profileMask)`
(ADR-0004, M0 Testing Decisions) and covers it with the agreed 4-case matrix. Then run the
non-interactive `--frames=5` verification and confirm the machine-readable markers and exit codes.

Machine-resolvable result contract:
- success: `M0_OK` (3.3+ core profile) followed by `M0_DONE rendered 5 frames`, **exit 0**
- failure: `M0_FAIL <reason>` and **non-zero exit**

Context/GL behavior must match the M0 matrix: sRGB + MSAA 4 + depth 24 hints on the created
context; debug GL context off in release, on in dev only after verifying `GL_KHR_debug`
(ADR-0004).

**Blocked by:** 0003

**Status:** ready-for-agent

- [ ] `supportsRequiredContext(major, minor, profileMask)` exists as a dependency-free pure
      function (no fabricated context needed)
- [ ] Test matrix passes: 3.3 core → `true`; 4.6 core → `true`; 3.2 core → `false`;
      3.3 compatibility → `false`
- [ ] `./build/<preset>/noto3d-editor --frames=5` prints `M0_OK` (3.3+ core profile) then
      `M0_DONE rendered 5 frames` and exits 0
- [ ] Failure path prints `M0_FAIL <reason>` and exits non-zero
- [ ] sRGB + MSAA 4 + depth 24 hints are applied on the created context
- [ ] Debug GL context is off in release; on in dev only after `GL_KHR_debug` is verified present