// Ground movement with round, exactly representable parameters, so every
// expected value below was worked out by hand (see the comments) rather than
// taken from the implementation's own output.
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
  p.walk_init = Fixed::from_double(0.25);
  p.walk_accel = Fixed::from_double(0.125);
  p.walk_max = Fixed::from_double(1.0);
  p.dash_init = Fixed::from_double(1.5);
  p.dash_accel = Fixed::from_double(0.125);
  p.dash_max = Fixed::from_double(2.0);
  p.dash_frames = 4;
  p.run_accel = Fixed::from_double(0.125);
  p.run_max = Fixed::from_double(2.0);
  p.skid_traction = Fixed::from_double(0.25);
  p.skid_frames = 3;
  p.turnaround_frames = 4;
  p.traction = Fixed::from_double(0.0625);
  return p;
}

struct Expect {
  double x;  // input this frame
  Mode mode;
  double px;
  double vx;
};

// Runs `frames` from rest at the origin and checks mode, x position and x
// velocity after every frame.
void run(const char* name, const std::vector<Expect>& frames, EntityState e = {}) {
  const MovementParams p = test_params();
  const Ground g{Fixed::zero()};
  int n = 0;
  for (const Expect& f : frames) {
    ++n;
    fp::step(e, Intent{Fixed::from_double(f.x)}, p, g);
    if (e.mode != f.mode || e.pos.x != Fixed::from_double(f.px) || e.vel.x != Fixed::from_double(f.vx)) {
      std::fprintf(stderr, "%s frame %d: got mode %d pos %.6f vel %.6f; want mode %d pos %.6f vel %.6f\n",
                   name, n, static_cast<int>(e.mode), e.pos.x.to_double(), e.vel.x.to_double(),
                   static_cast<int>(f.mode), f.px, f.vx);
      ++fp_test::failures();
      return;
    }
  }
}

const Mode Idle = Mode::Idle, Walk = Mode::Walk, Dash = Mode::Dash, Run = Mode::Run,
           Skid = Mode::Skid, Turn = Mode::Turnaround;

// x = 0.5 caps the walk at 0.5 * walk_max. Release: traction 0.0625 per frame,
// v_k = 0.5 - 0.0625k, sum over k=1..8 is 1.75, so it stops at 2.125 + 1.75.
void test_walk() {
  run("walk", {{0.5, Walk, 0.25, 0.25},
               {0.5, Walk, 0.625, 0.375},
               {0.5, Walk, 1.125, 0.5},
               {0.5, Walk, 1.625, 0.5},
               {0.5, Walk, 2.125, 0.5},
               {0, Idle, 2.5625, 0.4375},
               {0, Idle, 2.9375, 0.375},
               {0, Idle, 3.25, 0.3125},
               {0, Idle, 3.5, 0.25},
               {0, Idle, 3.6875, 0.1875},
               {0, Idle, 3.8125, 0.125},
               {0, Idle, 3.875, 0.0625},
               {0, Idle, 3.875, 0},
               {0, Idle, 3.875, 0}});
  // Left is the mirror image.
  run("walk left", {{-0.5, Walk, -0.25, -0.25}, {-0.5, Walk, -0.625, -0.375}, {-0.5, Walk, -1.125, -0.5}});
  // Inside the deadzone is neutral.
  run("deadzone", {{0.25, Idle, 0, 0}, {-0.25, Idle, 0, 0}});
}

// Leaving neutral and reaching the dash threshold within flick_frames (2) is a
// dash; reaching it more slowly is a walk.
void test_flick() {
  run("slow tilt walks", {{0.5, Walk, 0.25, 0.25}, {0.5, Walk, 0.625, 0.375}, {1.0, Walk, 1.125, 0.5}});
  run("fast tilt dashes", {{0.5, Walk, 0.25, 0.25}, {1.0, Dash, 1.75, 1.5}});
  run("digital press dashes", {{1.0, Dash, 1.5, 1.5}});
  // Out-of-range input is clamped to [-1, 1].
  run("clamped", {{5.0, Dash, 1.5, 1.5}, {5.0, Dash, 3.125, 1.625}});
  run("clamped left", {{-5.0, Dash, -1.5, -1.5}});
}

// Dash 4 frames: v = 1.5, 1.625, 1.75, 1.875, then Run: 2.0 (capped), 2.0.
// Release: skid_traction 0.25 from the Run frame: 1.75, 1.5, ..., 0.
void test_dash_run_skid() {
  run("dash run skid",
      {{1, Dash, 1.5, 1.5},
       {1, Dash, 3.125, 1.625},
       {1, Dash, 4.875, 1.75},
       {1, Run, 6.75, 1.875},  // dash_frames reached: the next frame is a Run frame
       {1, Run, 8.75, 2.0},
       {1, Run, 10.75, 2.0},
       {0, Skid, 12.5, 1.75},
       {0, Skid, 14.0, 1.5},
       {0, Skid, 15.25, 1.25},
       {0, Skid, 16.25, 1.0},
       {0, Skid, 17.0, 0.75},
       {0, Skid, 17.5, 0.5},
       {0, Skid, 17.75, 0.25},
       {0, Idle, 17.75, 0}});
}

// Releasing during a dash decelerates by traction and, at dash end, skids.
void test_dash_release() {
  run("dash release",
      {{1, Dash, 1.5, 1.5},
       {0, Dash, 2.9375, 1.4375},
       {0, Dash, 4.3125, 1.375},
       {0, Skid, 5.625, 1.3125},
       {0, Skid, 6.6875, 1.0625}});
}

// A run turnaround keeps its momentum but sheds it at skid_traction for
// turnaround_frames (4), facing flips at once, then runs the other way: against
// the new direction the speed gains run_accel + traction = 0.1875 per frame.
void test_turnaround() {
  run("turnaround",
      {{1, Dash, 1.5, 1.5},
       {1, Dash, 3.125, 1.625},
       {1, Dash, 4.875, 1.75},
       {1, Run, 6.75, 1.875},
       {1, Run, 8.75, 2.0},
       {1, Run, 10.75, 2.0},
       {-1, Turn, 12.5, 1.75},
       {-1, Turn, 14.0, 1.5},
       {-1, Turn, 15.25, 1.25},
       {-1, Run, 16.25, 1.0},  // turnaround_frames reached: the next frame runs left
       {-1, Run, 17.0625, 0.8125},
       {-1, Run, 17.6875, 0.625}});
  // Facing flips on entering the turnaround.
  EntityState e;
  const MovementParams p = test_params();
  const Ground g{Fixed::zero()};
  for (int i = 0; i < 6; ++i) fp::step(e, Intent{Fixed::one()}, p, g);
  CHECK_EQ(e.facing, 1);
  fp::step(e, Intent{-Fixed::one()}, p, g);
  CHECK_EQ(e.facing, -1);
  CHECK(e.mode == Turn);
}

// A flick the other way during a dash restarts it facing the other way.
void test_dash_dance() {
  run("dash dance", {{1, Dash, 1.5, 1.5}, {-1, Dash, 0.0, -1.5}, {-1, Dash, -1.625, -1.625}});
}

// A skid can be cancelled into a dash only after skid_frames (3).
void test_skid_cancel() {
  const std::vector<Expect> to_skid = {{1, Dash, 1.5, 1.5}, {1, Dash, 3.125, 1.625}, {1, Dash, 4.875, 1.75},
                                       {1, Run, 6.75, 1.875}, {1, Run, 8.75, 2.0}, {1, Run, 10.75, 2.0},
                                       {0, Skid, 12.5, 1.75}};
  auto with = [&](std::vector<Expect> tail) {
    auto v = to_skid;
    v.insert(v.end(), tail.begin(), tail.end());
    return v;
  };
  // Too early (skid frame 2 of 3): the flick is ignored and the skid goes on.
  run("early cancel", with({{-1, Skid, 14.0, 1.5}}));
  // Skid frames 1, 2, 3 have passed; the flick starts a left dash from 1.25.
  run("skid cancel", with({{0, Skid, 14.0, 1.5}, {0, Skid, 15.25, 1.25}, {-1, Dash, 13.75, -1.5}}));
}

// Moving with no input and no mode of its own: traction only, never past zero.
void test_idle_traction() {
  EntityState e;
  e.vel.x = Fixed::from_double(0.125);
  run("idle traction", {{0, Idle, 0.0625, 0.0625}, {0, Idle, 0.0625, 0}, {0, Idle, 0.0625, 0}}, e);
}

void test_ground_snap() {
  EntityState e;
  e.pos.y = Fixed::from_int(5);
  fp::step(e, Intent{}, test_params(), Ground{Fixed::from_int(3)});
  CHECK(e.pos.y == Fixed::from_int(3));
}

}  // namespace

int main() {
  test_walk();
  test_flick();
  test_dash_run_skid();
  test_dash_release();
  test_turnaround();
  test_dash_dance();
  test_skid_cancel();
  test_idle_traction();
  test_ground_snap();
  return fp_test::finish();
}
