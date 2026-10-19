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
| Body | box (half width, height; feet at the position), pass-through drop frames. Weight is deferred until a contact rule needs it |

Every field is a fixed-point value in units per frame or per frame squared,
documented with its Smash analogue.

## Ground movement (implemented)

`fp::step(EntityState&, Intent, MovementParams, Ground)` advances one entity
one frame (`include/fp/movement.hpp`). Intents are abstract: `move_x` in
[-1, 1] (clamped). Modes: Idle, Walk, Dash, Run, Skid, Turnaround. A mode's
`frames` counts frames spent in it including the current one, so
`dash_frames = 15` is a 15-frame dash.

- **Deadzone and flick.** `|axis| <= deadzone` is neutral. A *flick* is
  leaving neutral (or switching side) and reaching `dash_threshold` within
  `flick_frames`; a flick starts a dash from Idle, Walk or (after
  `skid_frames`) Skid, and restarts it the other way during a Dash (dash
  dance). Reaching the threshold more slowly walks.
- **Walk.** Starts at `walk_init` (or the current speed if higher), then
  accelerates by `walk_accel` toward `walk_max * |axis|`; above that cap it
  sheds `traction` per frame, never below the cap. Against the current
  velocity the acceleration is `walk_accel + traction`.
- **Dash.** Starts at `dash_init` (or the current speed if higher), accelerates
  by `dash_accel` to `dash_max` while the stick holds the dash direction, else
  sheds `traction`. After `dash_frames`: Run if the stick still holds that
  direction, else Skid (or Idle if stopped).
- **Run.** Accelerates by `run_accel` to `run_max`. Neutral starts a Skid
  (decelerating by `skid_traction`, ending at speed 0); the opposite
  direction starts a Turnaround: facing flips at once, momentum sheds at
  `skid_traction` for `turnaround_frames`, then the entity runs the other way.
- **Idle** sheds `traction` to zero. Grounded entities are placed on
  `ground.y`.

Parameters are Fixed data with Smash-like defaults, overridden per entity.

## Jumps and air movement (implemented)

Intents add `jump` (the button held; a *press* is the frame it becomes true,
tracked in the entity state so rollback needs nothing else) and `fast_fall`.

- **Jump squat.** A press from any ground mode starts a JumpSquat of
  `jump_squat_frames` (the press frame counts; 1 launches at once). Ground
  speed sheds `traction` during it. On the last frame the entity launches at
  `full_hop_vel` if the button is still held, else `short_hop_vel`, keeping
  `jump_x_retention` of its horizontal speed.
- **Air.** Horizontal: accelerate by `air_accel_base + air_accel_add * |axis|`
  toward `air_max * |axis|`; above that cap shed `air_friction` per frame
  (never below the cap); against the current velocity the acceleration gains
  `air_friction`; with no input shed `air_friction` toward zero.
- **Vertical.** Gravity pulls `vel.y` down to `-max_fall` and no further; a
  speed already beyond the cap is left alone. Fast-fall (once `vel.y <= 0`)
  sets the fall speed to `-fast_fall` and holds it until landing or an
  aerial jump.
- **Aerial jumps.** A press while airborne with fewer than `aerial_jumps`
  used sets `vel.y = aerial_jump_vel` (that frame has no gravity) and cancels
  fast-fall. Holding the button never repeats a jump.
- **Landing.** When falling (`vel.y <= 0`) and the integrated `pos.y <= ground.y`
  the entity is placed on the ground, `vel.y` zeroed, the aerial jumps are
  refilled, and the mode is Idle; horizontal speed is kept. (Landing lag
  belongs with the game's move system.)

## Platforms and contacts (implemented: `include/fp/world.hpp`)

`Config` is what a game sets up once (a `Stage` of platforms and a blast-zone
rectangle, plus one `MovementParams` per entity); `World` is all the
simulation state (the entities and a frame counter) and is plain data.
`fp::step(cfg, world, intents, events)` runs a frame; `events` is replaced
each frame, in a deterministic order (entity id).

Per entity, in id order: a fresh down press on a pass-through platform drops
through it; `advance` (motion, the state machine of the previous sections);
then contacts against the stage, which is static during contact resolution:

- **Walls.** Solid platforms stop horizontal motion when the body's side
  crosses their side while level with them; `vel.x` is zeroed.
- **Landing.** An airborne entity lands on the highest platform top its feet
  crossed this frame (and whose x-range the body overlaps); pass-through
  platforms are ignored for `drop_through_frames` after a drop. Landing sets
  the entity Idle, refills aerial jumps and records `support`.
- **Head bump.** Rising into a solid's underside stops the head there and
  zeroes `vel.y`. Pass-through platforms are open from below.
- **Depenetration.** If the body is still inside a solid (placed there, or a
  platform moved or respawned onto it) it is pushed out the shallowest way
  (up preferred on ties).
- **Support.** A grounded entity must still overlap its support's x-range at
  its top; otherwise it falls (`Fell`), keeping horizontal speed and aerial
  jumps. A fresh down press while standing on a pass-through platform drops
  through it.
- **Elimination.** Leaving the blast rectangle sets `eliminated` and emits
  `Eliminated`; eliminated entities are frozen until the game replaces them.

Events: `Landed`, `Fell`, `Eliminated`, `PlatformShaking`, `PlatformCrumbled`,
`PlatformRespawned`, `PlatformDestroyed`. `step` appends; the caller clears the
buffer (so events from `destroy_platform` between steps are not lost).

### Platform lifecycle (phase 6, after all entities)

`World::platforms` holds each platform's phase, timer, motion clock and
offset. Contacts in a frame see the platforms as the previous frame left
them, so an entity's contacts never depend on the order of other entities.

- **Crumble.** `shake_frames > 0`: the frame an entity is found standing on a
  Solid platform it starts Shaking (still solid) for `shake_frames`, then is
  Gone for `respawn_frames` (0 = for good), then Solid again. Riders fall when
  it goes (`Fell`). If it respawns on an entity, the entity is pushed out the
  shallowest way (usually up and landed).
- **Motion.** `move_period` (even) frames for a there-and-back trip of
  `travel` from the rest rectangle, linear. Riders (entities supported by the
  platform) are moved by the platform's displacement.
- **Destroy.** `destroy_platform` removes it permanently (riders fall).

## Tests

`tests/*_test.cpp` are plain CTest executables with a small check helper.
Movement is covered twice: unit tests with exactly representable parameters,
whose per-frame expectations are worked out by hand, and golden traces
(`tests/golden/*.txt`, raw Q16.16 per frame, default parameters) that pin the
whole pipeline. After an intended behaviour change, regenerate the traces
with `FP_UPDATE_GOLDEN=1 ./build/fighter_physics_golden_test` and review the
diff.

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
