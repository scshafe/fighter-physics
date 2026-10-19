// Jumps and air movement with exactly representable parameters; every
// expected number was worked out by hand (comments give the arithmetic).
#include <cstdio>
#include <vector>

#include "check.hpp"
#include "fp/movement.hpp"

using fp::EntityState;
using fp::Fixed;
using fp::Ground;
using fp::Intent;
using fp::Mode;
using fp::MovementParams;

namespace {

MovementParams test_params() {
  MovementParams p;
  p.deadzone = Fixed::from_double(0.25);
  p.dash_threshold = Fixed::from_double(0.75);
  p.flick_frames = 2;
  p.dash_init = Fixed::from_double(1.5);
  p.dash_accel = Fixed::from_double(0.125);
  p.dash_max = Fixed::from_double(2.0);
  p.dash_frames = 4;
  p.run_accel = Fixed::from_double(0.125);
  p.run_max = Fixed::from_double(2.0);
  p.traction = Fixed::from_double(0.0625);
  p.jump_squat_frames = 3;
  p.full_hop_vel = Fixed::from_double(2.0);
  p.short_hop_vel = Fixed::from_double(1.0);
  p.jump_x_retention = Fixed::from_double(0.5);
  p.aerial_jumps = 1;
  p.aerial_jump_vel = Fixed::from_double(1.5);
  p.air_accel_base = Fixed::from_double(0.0625);
  p.air_accel_add = Fixed::from_double(0.0625);
  p.air_max = Fixed::from_double(1.0);
  p.air_friction = Fixed::from_double(0.03125);
  p.gravity = Fixed::from_double(0.25);
  p.max_fall = Fixed::from_double(1.0);
  p.fast_fall = Fixed::from_double(2.0);
  return p;
}

const Mode Idle = Mode::Idle, Squat = Mode::JumpSquat, Air = Mode::Airborne, Dash = Mode::Dash;

struct Row {
  double x;
  bool jump, ff;
  Mode mode;
  double px, py, vx, vy;
};

void run(const char* name, const std::vector<Row>& rows, EntityState e = {}, MovementParams p = test_params()) {
  const Ground g{Fixed::zero()};
  int n = 0;
  for (const Row& r : rows) {
    ++n;
    fp::step(e, Intent{Fixed::from_double(r.x), r.jump, r.ff}, p, g);
    if (e.mode != r.mode || e.pos.x != Fixed::from_double(r.px) || e.pos.y != Fixed::from_double(r.py) ||
        e.vel.x != Fixed::from_double(r.vx) || e.vel.y != Fixed::from_double(r.vy)) {
      std::fprintf(stderr,
                   "%s frame %d: got mode %d pos (%.6f, %.6f) vel (%.6f, %.6f); want mode %d pos (%.6f, %.6f) "
                   "vel (%.6f, %.6f)\n",
                   name, n, static_cast<int>(e.mode), e.pos.x.to_double(), e.pos.y.to_double(),
                   e.vel.x.to_double(), e.vel.y.to_double(), static_cast<int>(r.mode), r.px, r.py, r.vx, r.vy);
      ++fp_test::failures();
      return;
    }
  }
}

EntityState airborne(double px, double py, double vx, double vy) {
  EntityState e;
  e.mode = Air;
  e.pos = fp::Vec2::from_double(px, py);
  e.vel = fp::Vec2::from_double(vx, vy);
  return e;
}

// Squat of 3 frames (press frame included), launch on the third at 2.0, then
// gravity 0.25: vy 1.75, 1.5, ..., 0 (apex y = 2+1.75+...+0.25+0 = 9.0), then
// falling to the cap of 1.0 and a landing on the frame y would pass 0.
void test_full_hop() {
  run("full hop",
      {{0, true, false, Squat, 0, 0, 0, 0},
       {0, true, false, Squat, 0, 0, 0, 0},
       {0, true, false, Air, 0, 2.0, 0, 2.0},  // launch: held at the last squat frame
       {0, true, false, Air, 0, 3.75, 0, 1.75},
       {0, true, false, Air, 0, 5.25, 0, 1.5},
       {0, true, false, Air, 0, 6.5, 0, 1.25},
       {0, true, false, Air, 0, 7.5, 0, 1.0},
       {0, true, false, Air, 0, 8.25, 0, 0.75},
       {0, true, false, Air, 0, 8.75, 0, 0.5},
       {0, true, false, Air, 0, 9.0, 0, 0.25},
       {0, true, false, Air, 0, 9.0, 0, 0},  // apex
       {0, true, false, Air, 0, 8.75, 0, -0.25},
       {0, true, false, Air, 0, 8.25, 0, -0.5},
       {0, true, false, Air, 0, 7.5, 0, -0.75},
       {0, true, false, Air, 0, 6.5, 0, -1.0},
       {0, true, false, Air, 0, 5.5, 0, -1.0},  // at max_fall
       {0, true, false, Air, 0, 4.5, 0, -1.0},
       {0, true, false, Air, 0, 3.5, 0, -1.0},
       {0, true, false, Air, 0, 2.5, 0, -1.0},
       {0, true, false, Air, 0, 1.5, 0, -1.0},
       {0, true, false, Air, 0, 0.5, 0, -1.0},
       {0, true, false, Idle, 0, 0, 0, 0},  // would reach -0.5: lands on the ground
       {0, true, false, Idle, 0, 0, 0, 0}});  // holding jump does not jump again
}

// Released before the squat ends: short hop, launch at 1.0, apex 1+.75+.5+.25 = 2.5,
// back to y = 0 exactly on the eleventh frame.
void test_short_hop() {
  run("short hop",
      {{0, true, false, Squat, 0, 0, 0, 0},
       {0, true, false, Squat, 0, 0, 0, 0},
       {0, false, false, Air, 0, 1.0, 0, 1.0},
       {0, false, false, Air, 0, 1.75, 0, 0.75},
       {0, false, false, Air, 0, 2.25, 0, 0.5},
       {0, false, false, Air, 0, 2.5, 0, 0.25},
       {0, false, false, Air, 0, 2.5, 0, 0},
       {0, false, false, Air, 0, 2.25, 0, -0.25},
       {0, false, false, Air, 0, 1.75, 0, -0.5},
       {0, false, false, Air, 0, 1.0, 0, -0.75},
       {0, false, false, Idle, 0, 0, 0, 0}});
  // A squat of one frame launches on the press frame.
  MovementParams p = test_params();
  p.jump_squat_frames = 1;
  run("one frame squat", {{0, true, false, Air, 0, 2.0, 0, 2.0}}, {}, p);
}

// One aerial jump (1.5, replacing gravity that frame); a second is refused.
void test_aerial_jump() {
  run("aerial jump",
      {{0, true, false, Squat, 0, 0, 0, 0},
       {0, true, false, Squat, 0, 0, 0, 0},
       {0, true, false, Air, 0, 2.0, 0, 2.0},
       {0, true, false, Air, 0, 3.75, 0, 1.75},
       {0, false, false, Air, 0, 5.25, 0, 1.5},
       {0, true, false, Air, 0, 6.75, 0, 1.5},  // pressed again: aerial jump
       {0, false, false, Air, 0, 8.0, 0, 1.25},
       {0, true, false, Air, 0, 9.0, 0, 1.0}});  // pressed again: none left, gravity
  // A press is the frame the button becomes held; holding it afterwards does not repeat the jump.
  run("aerial jump press then hold",
      {{0, true, false, Air, 0, 10.0, 0, 1.5}, {0, true, false, Air, 0, 11.25, 0, 1.25}}, airborne(0, 8.5, 0, 0.5));
}

// Jumping from a slide keeps jump_x_retention (0.5) of the ground speed left
// after the squat's traction (1.0 - 3 * 0.0625 = 0.8125 -> 0.40625).
void test_jump_momentum() {
  EntityState e;
  e.vel.x = Fixed::from_double(1.0);
  run("jump momentum",
      {{0, true, false, Squat, 0.9375, 0, 0.9375, 0},
       {0, true, false, Squat, 1.8125, 0, 0.875, 0},
       {0, true, false, Air, 2.21875, 2.0, 0.40625, 2.0}},
      e);
}

// Jumps start from every ground mode (here: a dash).
void test_jump_from_dash() {
  run("jump from dash",
      {{1, false, false, Dash, 1.5, 0, 1.5, 0},
       {1, true, false, Squat, 2.9375, 0, 1.4375, 0}});  // traction applies in the squat
}

// Air acceleration is base + add * |axis| = 0.125 at full tilt, toward
// air_max * |axis|; above the cap air_friction (1/32) is shed; against the
// current velocity the acceleration is accel + friction; neutral sheds friction.
void test_air_drift() {
  run("air accel", {{1, false, false, Air, 2.75, 10.0 - 0.25, 0.53125, -0.25}, {1, false, false, Air, 3.40625, 9.25, 0.65625, -0.5}},
      airborne(2.21875, 10, 0.40625, 0));
  run("air cap", {{1, false, false, Air, 1.46875, 9.75, 1.46875, -0.25}}, airborne(0, 10, 1.5, 0));
  run("air half tilt cap", {{0.5, false, false, Air, 0.5, 9.75, 0.5, -0.25}}, airborne(0, 10, 0.5, 0));
  run("air reverse", {{1, false, false, Air, -0.34375, 9.75, -0.34375, -0.25}}, airborne(0, 10, -0.5, 0));
  run("air friction", {{0, false, false, Air, 0.375, 9.75, 0.375, -0.25}, {0, false, false, Air, 0.71875, 9.25, 0.34375, -0.5}},
      airborne(0, 10, 0.40625, 0));
  run("air friction stops", {{0, false, false, Air, 0, 9.75, 0, -0.25}, {0, false, false, Air, 0, 9.25, 0, -0.5}},
      airborne(0, 10, 0.015625, 0));
  // Leaving the cap behind by tilting less sheds the excess.
  run("air less tilt", {{0.5, false, false, Air, 0.96875, 9.75, 0.96875, -0.25}}, airborne(0, 10, 1.0, 0));
}

// Fast-fall is ignored while rising, then sets the fall speed to fast_fall.
void test_fast_fall() {
  run("fast fall",
      {{0, false, true, Air, 0, 10.25, 0, 0.25},  // rising (vy 0.5 -> 0.25): ignored
       {0, false, true, Air, 0, 10.25, 0, 0},
       {0, false, true, Air, 0, 8.25, 0, -2.0},  // vy <= 0: fast falling
       {0, false, true, Air, 0, 6.25, 0, -2.0},
       {0, false, false, Air, 0, 4.25, 0, -2.0},  // stays fast falling after release
       {0, false, false, Air, 0, 2.25, 0, -2.0},
       {0, false, false, Air, 0, 0.25, 0, -2.0},
       {0, false, false, Idle, 0, 0, 0, 0}},
      airborne(0, 10, 0, 0.5));
  // An aerial jump cancels the fast fall.
  run("jump cancels fast fall",
      {{0, false, true, Air, 0, 8.0, 0, -2.0}, {0, true, false, Air, 0, 9.5, 0, 1.5}, {0, false, false, Air, 0, 10.75, 0, 1.25}},
      airborne(0, 10, 0, -0.25));
}

// The fall cap: gravity brings vy up to -max_fall and no further; a speed
// already beyond it is not touched.
void test_terminal() {
  run("terminal", {{0, false, false, Air, 0, 99.0, 0, -1.0}, {0, false, false, Air, 0, 98.0, 0, -1.0}},
      airborne(0, 100, 0, -0.75));
  run("beyond terminal", {{0, false, false, Air, 0, 97.0, 0, -3.0}}, airborne(0, 100, 0, -3.0));
}

// Landing resets the aerial jumps, so they can be used again.
void test_landing_resets_jumps() {
  EntityState e = airborne(0, 0.5, 0, -1.0);
  e.jumps_used = 1;
  fp::step(e, Intent{}, test_params(), Ground{Fixed::zero()});
  CHECK(e.mode == Idle);
  CHECK_EQ(e.jumps_used, 0);
  CHECK(e.pos.y == Fixed::zero());
  // It lands on a raised ground too, and keeps its horizontal speed.
  EntityState f = airborne(0, 5.5, 0.5, -1.0);
  fp::step(f, Intent{}, test_params(), Ground{Fixed::from_int(5)});
  CHECK(f.mode == Idle);
  CHECK(f.pos.y == Fixed::from_int(5));
  CHECK(f.vel.x == Fixed::from_double(0.5 - 0.03125));
}

}  // namespace

int main() {
  test_full_hop();
  test_short_hop();
  test_aerial_jump();
  test_jump_momentum();
  test_jump_from_dash();
  test_air_drift();
  test_fast_fall();
  test_terminal();
  test_landing_resets_jumps();
  return fp_test::finish();
}
