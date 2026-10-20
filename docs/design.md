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
Floats appear only in explicit edge conversions.

How the claim is kept honest (all run by `./scripts/verify` and in CI):

- `scripts/check-no-float` (a CTest test) fails if `float`, `double` or the
  math library appears in `include/` or `src/` outside the edge conversions
  in `fixed.hpp` and `vec2.hpp`.
- Pinned values that every platform must reproduce: the movement golden
  traces, the state hash of a sample world, the snapshot bytes, the final
  hash of the busy 48-entity scenario, and `tests/golden/determinism_hashes.txt`,
  that scenario's hash every 25 frames (a divergence names the first frame at
  which a platform differs).
- `./scripts/verify` builds and runs everything twice, Debug and Release, and
  CI runs it on Linux with gcc and with clang and on macOS (arm64, clang).
- The parallel step is compared with the serial step on every frame for 1, 2,
  3, 4, 5 and 8 workers.

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

**Parallelism (implemented).** `fp::step(cfg, world, intents, events, jobs)`
takes an optional `JobSystem*`; without one it is the serial step. The
phases are:

1. Per entity, in parallel: the drop-through press, `advance` (motion), platform
   contacts against the per-step platform view, and the blast-zone check. An
   entity writes only itself and its own event buffer.
2. Entity-entity landing and bump decisions, in parallel: each reads the
   positions before and after phase 1 and a snapshot of who rides whom, and
   writes only itself.
3. Rider follow rounds, in parallel per round, each round reading the previous
   round's heights.
4. Platforms (crumble, respawn, motion, carrying riders), serially: they are
   few and touch their riders.

Per-entity event buffers are concatenated in entity id order, so the event
stream is the same for any worker count. There is no broadphase: contacts are
brute force over the platforms and entities, which is exact and cheap at the
sizes a platform fighter has; the rollback benchmark will say if that changes.
`tests/parallel_determinism_test.cpp` runs a busy 48-entity scenario serially
and with 1, 2, 3, 4, 5 and 8 workers and requires the same state hash and
event stream on every frame; its final hash is pinned across platforms.

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

### Entity-entity contact (phase 5b)

Bodies pass through each other side to side. Vertical contact, after every
entity has moved and met the platforms:

- **Landing.** An airborne entity whose feet cross another's head (and whose
  body overlaps it horizontally) lands on it: `LandedOnEntity`, and it
  *stands on* that entity (`support_entity`).
- **Bump.** A rising entity whose head crosses another's underside stops
  under it (`BumpedEntity`). Entities standing on you, directly or in a stack,
  are not bumped: you carry them.
- **Riding.** A rider follows the head it stands on (so a jump lifts the
  stack) and falls when the carrier is eliminated or moves out from under it
  (the bodies stop overlapping horizontally). Platforms carry riders of riders
  too. Stacks settle in at most one round per entity, each round reading the
  previous round's heights, and landing/bump decisions read only the
  positions before and after the entities' own motion, so ids and processing
  order do not change the result.

### Platform lifecycle (phase 6, after all entities)

`World::platforms` holds each platform's phase, timer, motion clock and
offset. Contacts in a frame see the platforms as the previous frame left
them, so an entity's contacts never depend on the order of other entities.

- **Crumble by touch.** `shake_frames > 0`: the frame an entity is found
  standing on a Solid platform it starts Shaking (still solid) for
  `shake_frames`, then is Gone for `respawn_frames` (0 = for good), then Solid
  again. Riders fall when it goes (`Fell`). If it respawns on an entity, the
  entity is pushed out the shallowest way (usually up and landed).
- **Crumble by stress.** `PlatformDef::stress.shake_at > 0` replaces the
  touch model with a per-platform stress in [0, 1] (`PlatformState::stress`,
  part of the snapshot and the hash). Each frame, after the entities have
  moved, every entity standing on the platform adds `still_rate` (|vel.x| <
  `still_speed`) or `moving_rate`. An entity on a stack of heads sends its
  rate down to the platform under the bottom of the stack, times
  `rider_share` once per head between them. An entity whose feet are on
  several platforms (same top as its y, body overlapping) splits its rate
  equally between them. With nobody on it the stress falls by `decay`.
  Phases follow the level: Solid below `shake_at` (`PlatformShaking` on
  entering Shaking; back to Solid if it decays), Shaking from `shake_at`,
  Doomed from `doom_at` (`PlatformDoomed`; sticky, no decay, and the stress
  also rises by `doom_rate` a frame), and at 1 it is Gone for `respawn_frames`
  with the stress reset to 0 (`PlatformCrumbled`, riders fall).
  `add_stress(cfg, world, platform, amount, events)` is the one-off increment
  (hard landing, stomp) the game calls between steps; it applies the phase
  changes at once. All rates are Fixed per frame, so the game converts from
  its per-second table.
- **Motion.** `move_period` (even) frames for a there-and-back trip of
  `travel` from the rest rectangle, linear. Riders (entities supported by the
  platform) are moved by the platform's displacement.
- **Belt.** `PlatformState::belt` (units a frame, signed; 0 = not a belt) is
  set by the game between steps and is in the snapshot and the hash. Each
  frame, after its own motion and before contacts, an entity in a grounded
  mode whose support is the platform, or who stands on a stack of heads whose
  bottom does, moves by `belt` along x (the support is read from the start of
  the frame, so entity order never matters). Walls stop it, an edge drops it,
  friction is unchanged, so standing still means being carried and walking
  against it is slower. The platform itself does not move.
- **Destroy.** `destroy_platform` removes it permanently (riders fall).

### Placeables

Things the game puts on the stage during a match live in `World::placeables`:
numbered slots (at most `kMaxPlaceables` = 64) of plain data, so they are in
the snapshot and the hash like everything else. `place(world, Placeable)`
takes the lowest free slot (or appends) and returns it, or -1 when full or the
rect is empty; `remove_placeable` frees one; `life > 0` makes it expire after
that many frames (`PlaceableRemoved`, with `Fell` for its riders). A free slot
is all zero, so a stale rect never reaches the hash.

For contacts a slot is a platform id: ids from `stage.platforms.size()` up are
placeable slots. `EntityState::support` and `Landed.other` name a wall as
`stage.platforms.size() + slot`.

- **Wall.** Solid, like a Solid platform (stood on, walked into, bumped from
  below, pushed out of the shallowest way if placed on an entity), but it
  takes no stress and does not move.
- **Slick and Sticky patches.** Ground patches that change how an entity moves
  while it stands on the ground (grounded modes, including a jump squat) with
  its feet inside the rect (edges included, `bottom <= y <= top`). They scale
  `traction` and `skid_traction` by `traction_scale` and `walk_max`,
  `dash_max` and `run_max` by `speed_scale`; speed above a cap is shed at the
  (scaled) traction per frame, so ice with a lower cap does not slow anyone.
  Overlapping patches multiply in slot order. The air is not affected. They
  are not platforms: a patch slot is never a `support`. Slick is the ice
  preset (traction below 1), Sticky the mud preset (traction above 1, speed
  below 1); the engine treats them alike. The patch is read from where the
  entity stood at the start of the frame, so patches never depend on the order
  of entities.
- **Spring.** A pad: after an entity's own motion and platform contacts, if it
  is on the ground (any grounded mode) with its feet in the rect, it is put in
  the air with `vel.y = launch.y` and `vel.x += launch.x` and `Sprung` (id:
  entity, other: slot) is emitted; a landing on the pad springs on the same
  frame. The lowest slot under the feet acts. `launch.y` must be positive
  (checked by `place` and `restore`), so the entity leaves the pad and does not
  spring until it lands again. Landing on another entity's head is not ground
  here.

## Tests

`tests/*_test.cpp` are plain CTest executables with a small check helper.
Movement is covered twice: unit tests with exactly representable parameters,
whose per-frame expectations are worked out by hand, and golden traces
(`tests/golden/*.txt`, raw Q16.16 per frame, default parameters) that pin the
whole pipeline. After an intended behaviour change, regenerate the traces
with `FP_UPDATE_GOLDEN=1 ./build/fighter_physics_golden_test` and review the
diff.

## Job system (implemented: `include/fp/jobs.hpp`)

`fp::JobSystem(workers)` runs `parallel_for(count, fn)` on a fixed number of
threads (the caller is worker 0; 1 means no threads). The partition is
static: worker `w` always takes indices `[count*w/W, count*(w+1)/W)`, and
a job writes only data owned by its index, so which thread ran what, and how
many threads there were, cannot change a result. It uses the standard
library only (`std::thread`, mutex, condition variable). It is the only
parallel primitive the step needs: per-entity phases are `parallel_for` over
entities, and anything that crosses entities is reduced afterwards in id
order.

## State hash (implemented: `include/fp/state.hpp`)

`fp::hash(const World&)` is a 64-bit hash of every state field in a fixed
order, as little-endian integers (FNV-1a with a final mix), identical on every
platform. The fields are listed once, in `src/archive.hpp`, and visited by
"archives"; the hash is one, and snapshot/restore are others, so they cannot
disagree about what the state is. Tests flip every field and require the hash
to change, and pin a value that CI checks on Linux and macOS.

## Rollback support (implemented: `include/fp/state.hpp`)

All simulation state is plain data in `World` (vectors of trivially copyable
structs); config is not state. `snapshot(world, bytes)` writes it as
platform-independent little-endian bytes (a 4-byte header, then the fields of
`src/archive.hpp`: about 37 bytes per entity), `restore(cfg, world, bytes)`
reads it back, and `hash(world)` gives the 64-bit hash. `restore` is the
trust boundary for resyncs: it checks the header, the length, the counts
against the config, and every enum, flag and index, and it leaves the world
untouched on failure. The ring of snapshots, input history and the decision of
when to roll back belong to netplay-kit's session; the engine guarantees that
restoring a snapshot and replaying the same inputs reproduces the original run
bit for bit, for any worker count (`tests/rollback_test.cpp`: replay from a
snapshot, repeated 8-frame rollbacks, byte-level mutation of snapshots,
a pinned snapshot hash checked on Linux and macOS).

### Cost (`bench/rollback_bench.cpp`)

Release build, gcc, AMD Ryzen 5 5600G (12 threads); indicative only, rerun on
the target machines. Times in microseconds.

| Entities | Workers | Snapshot | Restore | Hash | Step | Rollback of 8 frames (restore, 8 steps, hash) |
| --- | --- | --- | --- | --- | --- | --- |
| 8 | 1 | 0.7 | 0.1 | 0.3 | 0.6 | 6 |
| 8 | 4 | 0.7 | 0.1 | 0.4 | 24 | 254 |
| 48 | 1 | 3.1 | 0.5 | 1.6 | 6.7 | 68 |
| 48 | 4 | 3.2 | 0.5 | 1.6 | 51 | 459 |
| 256 | 1 | 15.8 | 2.3 | 8.3 | 109 | 1616 |
| 256 | 4 | 16.0 | 2.4 | 8.3 | 125 | 1307 |

Rolling back 8 frames is negligible (0.04% of a 60 fps frame) at fighter-game
sizes, so the serial step is the right default there. The parallel step only
starts to pay off in the hundreds of entities: each parallel phase costs a
thread wake-up (about 6-12 us), and the step has four. The job system could
spin briefly before blocking to cut that; it is not done because nothing needs
it yet. There is no broadphase for the same reason: the brute-force contact
loops are not the cost at these sizes.

## Out of scope

Rendering, input devices, audio, networking, game rules (stocks, scoring) and
assets. Those belong to the game (ledgefall) or to netplay-kit.

## Milestones

1. Done: the fixed-point scalar and vector types, with exhaustive tests.
2. Done: one entity on flat ground and in the air with the full movement table, plus golden traces.
3. Done: platforms (solid, pass-through, moving, crumbling, respawning, destroyed) and contacts, including vertical-only body contact between entities.
4. Done: the job system and the deterministic parallel step: hash equality across worker counts.
5. Done: snapshot, restore and hash, with a rollback benchmark.
6. Done: the macOS CI job and the cross-platform determinism check.

7. Done: crumbling by accumulated stress, and placeables (walls, slick and sticky patches, springs).

Next, only if the game's rules need them: entity weight, slopes, landing lag
and other move-driven state that belongs with the game's move system, and a
faster job system if the entity count ever needs it.
