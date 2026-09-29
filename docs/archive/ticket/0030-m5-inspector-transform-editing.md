# 0030 — M5 Inspector: numeric transform editing in degrees, commit-on-release

**Source:** ADR-0011 M5 spec (Q8 / Q13); third M5 slice, parallel to 0031.
**Status:** implemented — machine-verified; manual evidence pending
**Blocked by:** 0029

The first slice that changes what the 3D view shows. Independent of 0031 (asset picker): this one
delivers "selected something → I can change where and how it sits", 0031 delivers "selected
something → I can change what it is".

## What it delivers

- The Inspector offers numeric fields for the selected entity's **position, rotation, and scale**.
- **Rotation is edited in degrees**, labelled by what the axes actually mean —
  **`Pitch (X)` / `Yaw (Y)` / `Roll (Z)`** — with the applied order (`Y → X → Z`) shown, not left
  to be inferred from the letters.
- Edits take effect **immediately**; the 3D view reflects what was typed with no extra confirm step.
- Each committed edit is observable in the structured log, recording **both** the displayed value
  and the stored value.

## Contracts this slice must honour

- **The Scene keeps radians and stays untouched.** `Transform::rotation` and the single
  `modelMatrix` definition are unchanged; degrees are a **presentation-layer conversion** in the UI
  adapter — the same principle that keeps pixel→radian mapping in the app/input layer and
  `renderer::Camera` sensitivity-agnostic (Q13, ADR-0010 Q7).
- **Field labels state the role, not just the letter.** `modelMatrix` composes
  `eulerAngleYXZ(rotation.y, rotation.x, rotation.z)`, so storage letter order is **not**
  application order; a panel labelled only "X / Y / Z" misleads whenever two axes are non-zero
  (Q13).
- **The Scene value is the widget's source of truth every frame**: read → convert to degrees → feed
  the widget → commit back to radians. **No persistent degrees copy is kept in UI state** — that
  would be a second representation of the truth and invites per-frame write-back drift (Q13).
- **Commit boundary** (Q8): continuous widgets commit and log on
  `IsItemDeactivatedAfterEdit()` — on release, with the final value; discrete operations on the
  event. Never per frame, never on intermediate drag values.
- **Displayed precision is authoritative on commit (WYSIWYG)**: the value written is the displayed
  value converted back to radians, so quantisation at the displayed precision is really written to
  the Scene. This is recorded as a contract so it is never reported as a bug.
- **Logs record both units explicitly** (e.g. `value=90.000deg radians=1.5708`): logging only the
  stored value would make the evidence disagree with what the user saw, and logging only the
  displayed value would hide what the Scene holds (Q8).
- Edits go through **`Editor` semantic operations keyed by `EntityId`**; the inspector never
  mutates the Scene through a pointer of its own (Q3, Q11).
- The frozen M4 **window-title contract is not reopened** (Q8).
- **No gizmo, no on-viewport direct manipulation, no undo** (Q12, Q4). This slice is numeric fields
  only.

## Acceptance

- [ ] Position, rotation, and scale fields for the selected entity; a clear no-selection state
- [ ] Rotation displayed and entered in **degrees**, labelled `Pitch (X)` / `Yaw (Y)` / `Roll (Z)`,
      with the applied order `Y → X → Z` visible in the panel
- [ ] A typed value appears in the 3D view immediately; the same entity rotated by yaw, with two
      axes non-zero, behaves as the labels claim
- [ ] Rotation stored in the Scene is **radians**, and the single `modelMatrix` path is unchanged
      (existing matrix tests stay green and unmodified in spirit)
- [ ] A drag that ends on a value commits **once**, on release, with the final value logged — not
      once per frame
- [ ] Log lines carry both units; a manual check of one edit shows the displayed value and the
      stored value agreeing
- [ ] Re-opening the Inspector shows the Scene's value, not a remembered widget value
- [ ] Headless suite covers the degree↔radian conversion (including negatives and values past ±180°)
      and the commit function, which stays ImGui-free
- [ ] No draggable gizmo or handle appears in the 3D view; no undo history is retained
- [ ] M4 contracts unchanged: CLI exit-code matrix, `M4_INFO` lines, `xprop` titles
- [ ] Manual evidence recorded: editing each field, immediacy of feedback, and axis labelling

**Points to ADR:** 0011 (Q13 degrees-at-UI contract, Q8 commit boundary and logging, Q3/Q11 the
`EntityId` write path through the facade) · 0010 (Q3 radians YXZ `T·R·S` and TU-local
`GLM_ENABLE_EXPERIMENTAL`, Q7 unit-conversion ownership)
