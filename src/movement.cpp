#include "fp/movement.hpp"

namespace fp {
namespace {

int sign(Fixed v) noexcept { return v > Fixed::zero() ? 1 : (v < Fixed::zero() ? -1 : 0); }

// v along `dir` (+1/-1) as a signed speed, and back.
Fixed along(Fixed v, int dir) noexcept { return dir >= 0 ? v : -v; }

// Move |v| toward zero by `amount`, never past it.
Fixed decelerate(Fixed v, Fixed amount) noexcept {
  Fixed s = abs(v);
  s = s > amount ? s - amount : Fixed::zero();
  return v < Fixed::zero() ? -s : s;
}

// Speed `s` along the input direction, pushed toward `cap`: accelerate below
// it (never past it), shed `decel` per frame above it (never below it), and
// when moving against the input add `decel` to the acceleration.
Fixed approach_cap(Fixed s, Fixed cap, Fixed accel, Fixed decel) noexcept {
  if (s < Fixed::zero()) return min(s + accel + decel, cap);
  if (s < cap) return min(s + accel, cap);
  return max(s - decel, cap);
}

void enter(EntityState& e, Mode m) noexcept {
  e.mode = m;
  e.frames = 1;
}

}  // namespace

void advance(EntityState& e, const Intent& in, const MovementParams& p) noexcept {
  const Fixed x = clamp(in.move_x, -Fixed::one(), Fixed::one());
  const Fixed ax = abs(x);
  const int dir = ax > p.deadzone ? sign(x) : 0;

  // A flick is leaving neutral (or switching side) and reaching the dash
  // threshold within flick_frames.
  if (dir == 0) {
    e.flick_age = 0;
  } else if (dir != e.prev_dir) {
    e.flick_age = 1;
  } else if (e.flick_age < 255) {
    ++e.flick_age;
  }
  e.prev_dir = static_cast<std::int8_t>(dir);
  const bool jump_pressed = in.jump && e.prev_jump == 0;
  e.prev_jump = in.jump ? 1 : 0;
  e.prev_down = in.fast_fall ? 1 : 0;
  const bool flicked = dir != 0 && ax >= p.dash_threshold && e.flick_age <= p.flick_frames;

  auto start_dash = [&] {
    e.facing = static_cast<std::int8_t>(dir);
    enter(e, Mode::Dash);
    const Fixed s = along(e.vel.x, dir);
    e.vel.x = along(max(p.dash_init, s), dir);
  };
  auto start_walk = [&] {
    e.facing = static_cast<std::int8_t>(dir);
    enter(e, Mode::Walk);
    e.vel.x = along(max(p.walk_init, along(e.vel.x, dir)), dir);
  };

  // A jump press from any ground mode starts the jump squat on this frame.
  bool squat_started = false;
  if (jump_pressed && e.mode <= Mode::Turnaround) {
    enter(e, Mode::JumpSquat);
    squat_started = true;
  }

  switch (e.mode) {
    case Mode::Idle:
      if (dir != 0) {
        if (flicked) start_dash();
        else start_walk();
      } else {
        e.vel.x = decelerate(e.vel.x, p.traction);
      }
      break;

    case Mode::Walk:
      if (dir == 0) {
        enter(e, Mode::Idle);
        e.vel.x = decelerate(e.vel.x, p.traction);
      } else if (flicked) {
        start_dash();
      } else {
        ++e.frames;
        e.facing = static_cast<std::int8_t>(dir);
        e.vel.x = along(approach_cap(along(e.vel.x, dir), p.walk_max * ax, p.walk_accel, p.traction), dir);
      }
      break;

    case Mode::Dash:
      if (dir != 0 && dir != e.facing && flicked) {
        start_dash();  // dash dance: a flick the other way restarts the dash
        break;
      }
      ++e.frames;
      if (dir == e.facing) {
        e.vel.x = along(approach_cap(along(e.vel.x, dir), p.dash_max, p.dash_accel, p.traction), dir);
      } else {
        e.vel.x = decelerate(e.vel.x, p.traction);
      }
      if (e.frames >= p.dash_frames) {
        if (dir == e.facing) {
          enter(e, Mode::Run);
        } else if (e.vel.x != Fixed::zero()) {
          enter(e, Mode::Skid);
        } else {
          enter(e, Mode::Idle);
        }
      }
      break;

    case Mode::Run:
      if (dir == 0) {
        enter(e, Mode::Skid);
        e.vel.x = decelerate(e.vel.x, p.skid_traction);
      } else if (dir != e.facing) {
        e.facing = static_cast<std::int8_t>(dir);
        enter(e, Mode::Turnaround);
        e.vel.x = decelerate(e.vel.x, p.skid_traction);
      } else {
        ++e.frames;
        e.vel.x = along(approach_cap(along(e.vel.x, dir), p.run_max, p.run_accel, p.traction), dir);
      }
      break;

    case Mode::Skid:
      if (e.frames >= p.skid_frames && dir != 0) {
        if (flicked) start_dash();
        else start_walk();
        break;
      }
      ++e.frames;
      e.vel.x = decelerate(e.vel.x, p.skid_traction);
      if (e.vel.x == Fixed::zero()) enter(e, Mode::Idle);
      break;

    case Mode::Turnaround:
      ++e.frames;
      e.vel.x = decelerate(e.vel.x, p.skid_traction);
      if (e.frames >= p.turnaround_frames) {
        if (dir == e.facing) enter(e, Mode::Run);
        else if (dir == 0) enter(e, Mode::Skid);
        else enter(e, Mode::Idle);
      }
      break;

    case Mode::JumpSquat:
      if (!squat_started) ++e.frames;
      e.vel.x = decelerate(e.vel.x, p.traction);
      if (e.frames >= p.jump_squat_frames) {
        // Short hop if the button is no longer held when the squat ends.
        e.vel.y = in.jump ? p.full_hop_vel : p.short_hop_vel;
        e.vel.x *= p.jump_x_retention;
        enter(e, Mode::Airborne);
        e.jumps_used = 0;
        e.fast_falling = 0;
      }
      break;

    case Mode::Airborne:
      ++e.frames;
      // Vertical: an aerial jump replaces gravity for the frame; otherwise
      // fast-fall (only once not rising) or gravity up to the fall cap. A speed
      // already beyond the cap is left alone.
      if (jump_pressed && e.jumps_used < p.aerial_jumps) {
        e.vel.y = p.aerial_jump_vel;
        ++e.jumps_used;
        e.fast_falling = 0;
      } else if (e.fast_falling != 0 || (in.fast_fall && e.vel.y <= Fixed::zero())) {
        e.fast_falling = 1;
        e.vel.y = -p.fast_fall;
      } else if (e.vel.y > -p.max_fall) {
        e.vel.y = max(e.vel.y - p.gravity, -p.max_fall);
      }
      // Horizontal: accelerate toward air_max * |axis|, shed air_friction above
      // it, and with no input just shed air_friction.
      if (dir == 0) {
        e.vel.x = decelerate(e.vel.x, p.air_friction);
      } else {
        const Fixed accel = p.air_accel_base + p.air_accel_add * ax;
        e.vel.x = along(approach_cap(along(e.vel.x, dir), p.air_max * ax, accel, p.air_friction), dir);
      }
      break;
  }

  e.pos.x += e.vel.x;
  if (e.mode == Mode::Airborne) e.pos.y += e.vel.y;
}

void land(EntityState& e) noexcept {
  e.vel.y = Fixed::zero();
  enter(e, Mode::Idle);
  e.jumps_used = 0;
  e.fast_falling = 0;
}

void start_fall(EntityState& e) noexcept {
  e.vel.y = Fixed::zero();
  enter(e, Mode::Airborne);
  e.fast_falling = 0;
}

void step(EntityState& e, const Intent& in, const MovementParams& p, const Ground& g) noexcept {
  advance(e, in, p);
  if (e.mode == Mode::Airborne) {
    if (e.vel.y <= Fixed::zero() && e.pos.y <= g.y) {
      e.pos.y = g.y;
      land(e);
    }
  } else {
    e.pos.y = g.y;
  }
}

}  // namespace fp
