#pragma once
// One entity's movement: per-entity parameters (data, not code), abstract
// per-frame intents, plain trivially copyable state, and a fixed-timestep
// step. All values are Fixed: velocities in units/frame, accelerations in
// units/frame^2, times in frames, y up.
//
// Frames are counted as Smash counts them: a mode's `frames` is the number of
// frames spent in it including the current one, so `dash_frames = 15` means a
// dash lasts exactly 15 frames.

#include <cstdint>

#include "fp/vec2.hpp"

namespace fp {

// A movement archetype. Every field has a Smash analogue (named in the
// comments); the defaults are loosely Smash-like and are not any one
// character. Override per entity with designated initialisers.
struct MovementParams {
  // Input
  Fixed deadzone = Fixed::from_double(0.25);        // |axis| at or below this is neutral
  Fixed dash_threshold = Fixed::from_double(0.75);  // |axis| at or above this, flicked, is a dash
  std::int32_t flick_frames = 3;                    // max frames from leaving neutral to the threshold

  // Walk
  Fixed walk_init = Fixed::from_double(0.2);   // walk initial velocity
  Fixed walk_accel = Fixed::from_double(0.1);  // walk acceleration
  Fixed walk_max = Fixed::from_double(0.9);    // walk max speed (scaled by |axis|)

  // Dash
  Fixed dash_init = Fixed::from_double(1.6);    // dash initial velocity
  Fixed dash_accel = Fixed::from_double(0.1);   // dash acceleration
  Fixed dash_max = Fixed::from_double(1.76);    // dash max speed
  std::int32_t dash_frames = 15;                // dash length

  // Run
  Fixed run_accel = Fixed::from_double(0.1);    // run acceleration
  Fixed run_max = Fixed::from_double(1.76);     // run max speed
  Fixed skid_traction = Fixed::from_double(0.08);  // run-stop and turnaround deceleration
  std::int32_t skid_frames = 12;                // frames before a skid can be cancelled
  std::int32_t turnaround_frames = 10;          // run turnaround length

  // Ground
  Fixed traction = Fixed::from_double(0.06);    // ground deceleration (friction)

  // Jumps
  std::int32_t jump_squat_frames = 5;                 // jump squat length (>= 1)
  Fixed full_hop_vel = Fixed::from_double(2.6);       // full hop initial velocity (up)
  Fixed short_hop_vel = Fixed::from_double(1.7);      // short hop initial velocity
  Fixed jump_x_retention = Fixed::from_double(1.0);   // share of ground speed kept on leaving the ground
  std::int32_t aerial_jumps = 1;                      // aerial (mid-air) jumps
  Fixed aerial_jump_vel = Fixed::from_double(2.4);    // aerial jump velocity (up)

  // Air
  Fixed air_accel_base = Fixed::from_double(0.01);    // air acceleration, base
  Fixed air_accel_add = Fixed::from_double(0.03);     // air acceleration, extra at full tilt
  Fixed air_max = Fixed::from_double(1.0);            // max air speed (scaled by |axis|)
  Fixed air_friction = Fixed::from_double(0.01);      // air friction (deceleration)

  // Vertical
  Fixed gravity = Fixed::from_double(0.095);          // gravity
  Fixed max_fall = Fixed::from_double(1.5);           // max fall speed
  Fixed fast_fall = Fixed::from_double(2.4);          // fast-fall speed

  // Body: an axis-aligned box standing on its feet, `height` tall and
  // 2 * body_half_width wide, centred on the position (y up).
  Fixed body_half_width = Fixed::from_double(4.0);
  Fixed body_height = Fixed::from_double(12.0);
  std::int32_t drop_through_frames = 10;  // frames pass-through platforms are ignored after a drop
};

// Abstract intents for one frame. A game maps its controls to these.
struct Intent {
  Fixed move_x{};         // -1..1; positive is +x. Out-of-range values are clamped.
  bool jump = false;      // jump button held; a press is the frame it becomes true
  bool fast_fall = false; // down held: fast-fall in the air; a fresh press drops through a
                          // pass-through platform you stand on
};

enum class Mode : std::uint8_t { Idle, Walk, Dash, Run, Skid, Turnaround, JumpSquat, Airborne };

// All of an entity's simulation state: plain data, no pointers.
struct EntityState {
  Vec2 pos{};
  Vec2 vel{};
  Mode mode = Mode::Idle;
  std::int8_t facing = 1;      // -1 or +1
  std::int8_t prev_dir = 0;    // stick direction last frame (-1, 0, +1; neutral zone is 0)
  std::uint8_t flick_age = 0;  // frames since the stick left neutral or changed side (saturates)
  std::int32_t frames = 0;     // frames in `mode`, including the current one
  std::int8_t jumps_used = 0;  // aerial jumps used since leaving the ground
  std::uint8_t prev_jump = 0;  // jump button last frame (0/1)
  std::uint8_t fast_falling = 0;  // 0/1
  std::uint8_t prev_down = 0;     // fast_fall intent last frame (0/1)
  std::uint8_t eliminated = 0;    // 0/1: left the blast zone; frozen until the game replaces it
  std::int16_t support = -1;      // index of the platform stood on, or -1
  std::int16_t support_entity = -1;  // id of the entity stood on (its head), or -1
  std::int16_t drop_timer = 0;    // frames left ignoring pass-through platforms
};

// Motion only: apply the intent to the movement state machine and integrate
// position. No ground or platform is consulted; a grounded entity keeps its y.
// The caller resolves contacts afterwards (flat `step` below, or `fp::step`
// on a World in world.hpp).
void advance(EntityState& e, const Intent& in, const MovementParams& p) noexcept;

// Put an airborne entity in the landed state (Idle, aerial jumps refilled,
// vertical speed zero, horizontal speed kept).
void land(EntityState& e) noexcept;
// Put a grounded entity in the air without a jump (walking off an edge, a
// vanished platform): it keeps its aerial jumps and horizontal speed.
void start_fall(EntityState& e) noexcept;

// The ground the entity stands on (flat, infinite). The simplest world; also
// the reference the movement tests use.
struct Ground {
  Fixed y{};
};

// Advance one entity by one frame on flat ground.
void step(EntityState& e, const Intent& in, const MovementParams& p, const Ground& g) noexcept;

}  // namespace fp
