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
};

// Abstract intents for one frame. A game maps its controls to these.
struct Intent {
  Fixed move_x{};  // -1..1; positive is +x. Out-of-range values are clamped.
};

enum class Mode : std::uint8_t { Idle, Walk, Dash, Run, Skid, Turnaround };

// All of an entity's simulation state: plain data, no pointers.
struct EntityState {
  Vec2 pos{};
  Vec2 vel{};
  Mode mode = Mode::Idle;
  std::int8_t facing = 1;      // -1 or +1
  std::int8_t prev_dir = 0;    // stick direction last frame (-1, 0, +1; neutral zone is 0)
  std::uint8_t flick_age = 0;  // frames since the stick left neutral or changed side (saturates)
  std::int32_t frames = 0;     // frames in `mode`, including the current one
};

// The ground the entity stands on (flat, for now).
struct Ground {
  Fixed y{};
};

// Advance one entity by one frame.
void step(EntityState& e, const Intent& in, const MovementParams& p, const Ground& g) noexcept;

}  // namespace fp
