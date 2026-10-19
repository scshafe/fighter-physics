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

void step(EntityState& e, const Intent& in, const MovementParams& p, const Ground& g) noexcept {
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
  }

  e.pos.x += e.vel.x;
  e.pos.y = g.y;
}

}  // namespace fp
