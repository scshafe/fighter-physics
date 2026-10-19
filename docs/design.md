# fighter-physics: design (starting point)

The brief (`docs/brief.md`) is fixed; this document is the designer's, and the
project's agent keeps it in step with the code. Decisions that are hard to
reverse get a record in `docs/decisions/`.

## Shape

A C++20 static library, `fp::fighter_physics`, consumed with CMake
(`FetchContent` at a pinned commit, or `add_subdirectory`). Headers in
`include/fp/`, sources in `src/`, tests in `tests/` run by CTest.

## Determinism

Decided in `docs/decisions/0001-number-representation.md`: all simulation
state is fixed-point, `fp::Fixed` (Q16.16 in an `int32_t`). Products of two
quantities that are not Q16.16 (dot, cross, squared distance) return
`fp::Wide` (Q32.32 in an `int64_t`) so they cannot overflow into saturation.
Floats appear only in explicit edge conversions. A cross-platform
determinism test runs the same scenario on Linux (here and in CI) and on
macOS (CI) and compares state hashes.

## The step

A fixed timestep (frames, as Smash counts them), with optional substeps. Each
frame:

1. Apply inputs to per-entity intents. The engine knows buttons only as
   abstract intents such as move axis, jump and fast-fall; games map their
   controls.
2. Run the movement state machine per entity: grounded (walk, dash, run,
   skid/turnaround, traction) and airborne (air acceleration, air friction,
   gravity, fall and fast-fall caps, jumps).
3. Integrate.
4. Broadphase.
5. Resolve contacts: platforms, placeables, entity-entity vertical contact.
6. Update platform and placeable lifecycles: crumble timers, respawn, motion,
   destruction.
7. Emit events for the game layer: landed, fell, crumbled, eliminated.

**Parallelism.** A job system with a fixed worker count. Per-entity phases
(1-3) partition the entities. Broadphase and contacts are parallel but are
**reduced in a deterministic order**, sorted by stable entity ids, so the
result never depends on thread count or timing. Tests run the same scenario
with 1, 2 and N workers and require identical hashes.

## Movement parameters

One struct per entity archetype, overridable per entity. These fields are the
starting point:

| Group | Fields |
| --- | --- |
| Walk | initial velocity, acceleration, max speed |
| Dash | initial velocity, acceleration, max (dash) speed, dash length (frames) |
| Run | acceleration, max speed, skid/turnaround traction |
| Ground | traction (deceleration), slope behaviour (later) |
| Air | acceleration (base and additional), max horizontal speed, air friction |
| Vertical | gravity, max fall speed, fast-fall speed, terminal-velocity rules |
| Jumps | jump-squat frames, full-hop and short-hop velocities, aerial jump count and velocity |
| Body | collision shape (box/capsule), weight (for vertical contact) |

Every field is a fixed-point value in units per frame or per frame squared,
documented with its Smash analogue.

## Rollback support

All simulation state lives in plain, trivially copyable arrays. `snapshot()`
writes it into a byte buffer, `restore()` reads it back, and `hash()` gives a
stable 64-bit hash. Re-simulating N frames from a snapshot is the operation
netcode needs, so its cost is measured by a benchmark.

## Out of scope

Rendering, input devices, audio, networking, game rules (stocks, scoring) and
assets. Those belong to the game (ledgefall) or to netplay-kit.

## First milestones

1. The fixed-point scalar and vector types, with exhaustive tests.
2. One entity on flat ground with the full movement table, plus golden-trace tests.
3. Platforms (solid, pass-through, crumbling, respawning) and contacts.
4. The job system and the deterministic parallel step: hash equality across worker counts.
5. Snapshot, restore and hash, with a rollback benchmark.
6. The macOS CI job and the cross-platform determinism check.
