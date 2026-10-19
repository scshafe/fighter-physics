// Platform contacts on a World. Every scenario is a tiny stage with exactly
// representable numbers; the expected values were worked out by hand (the
// arithmetic is in the comments).
#include <cstdio>
#include <vector>

#include "check.hpp"
#include "fp/world.hpp"

using namespace fp;

namespace {

Fixed F(double v) { return Fixed::from_double(v); }

MovementParams params() {
  MovementParams p;
  p.deadzone = F(0.25);
  p.dash_threshold = F(0.75);
  p.flick_frames = 2;
  p.walk_init = F(0.25);
  p.walk_accel = F(0.125);
  p.walk_max = F(1.0);
  p.dash_init = F(1.5);
  p.dash_accel = F(0.125);
  p.dash_max = F(2.0);
  p.run_accel = F(0.125);
  p.run_max = F(2.0);
  p.traction = F(0.0625);
  p.jump_squat_frames = 3;
  p.full_hop_vel = F(2.0);
  p.short_hop_vel = F(1.0);
  p.air_accel_base = F(0.0625);
  p.air_accel_add = F(0.0625);
  p.air_max = F(1.0);
  p.air_friction = F(0.03125);
  p.gravity = F(0.25);
  p.max_fall = F(1.0);
  p.fast_fall = F(2.0);
  p.body_half_width = F(1.0);
  p.body_height = F(4.0);
  p.drop_through_frames = 3;
  return p;
}

PlatformDef solid(double l, double r, double b, double t) { return {PlatformKind::Solid, {F(l), F(r), F(b), F(t)}}; }
PlatformDef thin(double l, double r, double t) { return {PlatformKind::PassThrough, {F(l), F(r), F(t - 1), F(t)}}; }

Config config(std::vector<PlatformDef> platforms, int entities = 1) {
  Config c;
  c.stage.platforms = std::move(platforms);
  c.stage.blast = {F(-150), F(150), F(-50), F(100)};
  c.params.assign(entities, params());
  return c;
}

EntityState air(double x, double y, double vx, double vy) {
  EntityState e;
  e.mode = Mode::Airborne;
  e.pos = Vec2::from_double(x, y);
  e.vel = Vec2::from_double(vx, vy);
  return e;
}

// Steps entity 0 `n` times with `in`, returning the step (1-based) at which an
// event of `type` first appears, or 0.
struct Runner {
  Config cfg;
  World w;
  Events ev;
  int steps = 0;
  Runner(Config c, EntityState e) : cfg(std::move(c)) { w.entities.push_back(e); }
  const EntityState& e() const { return w.entities[0]; }
  bool step(Intent in = {}) {
    std::vector<Intent> v(w.entities.size(), in);
    fp::step(cfg, w, v, ev);
    ++steps;
    return true;
  }
  bool has(EventType t, int other = -2) const {
    for (const Event& x : ev)
      if (x.type == t && x.id == 0 && (other == -2 || x.other == other)) return true;
    return false;
  }
};

const Mode Idle = Mode::Idle, Airb = Mode::Airborne;

// Falling at the cap (1.0/frame) onto the floor from y=2.5: 1.5, 0.5, then -0.5 -> lands on y=0.
void test_landing() {
  Runner r(config({solid(-100, 100, -10, 0)}), air(0, 2.5, 0, -1));
  r.step();
  r.step();
  CHECK(r.e().mode == Airb && r.e().pos.y == F(0.5) && r.ev.empty());
  r.step();
  CHECK(r.e().mode == Idle && r.e().pos.y == F(0) && r.e().vel.y == F(0) && r.e().support == 0);
  CHECK(r.has(EventType::Landed, 0));
  r.step();  // standing: no events, stays put
  CHECK(r.e().mode == Idle && r.e().pos.y == F(0) && r.ev.empty());
}

// Walk off the right edge (x=100): body [x-1, x+1] stops overlapping when x >= 101.
// From 100.5 walking at half tilt: 100.75 (still on), 101.125 (off): Fell, then
// gravity. Falling from y=0: 0.25, 0.75, 1.5, 2.5, then 1 per frame; below the
// blast floor (-50) at -2.5 - 48 = -50.5 on the 52nd airborne frame, the 54th step.
void test_walk_off_and_eliminated() {
  Runner r(config({solid(-100, 100, -10, 0)}), spawn_on(config({solid(-100, 100, -10, 0)}), 0, F(100.5)));
  Intent walk{F(0.5)};
  r.step(walk);
  CHECK(r.e().mode == Mode::Walk && r.e().pos.x == F(100.75) && r.e().support == 0);
  r.step(walk);
  CHECK(r.e().mode == Airb && r.e().pos.x == F(101.125) && r.e().pos.y == F(0) && r.e().support == -1);
  CHECK(r.has(EventType::Fell));
  int eliminated_at = 0;
  for (int i = 3; i <= 80 && !eliminated_at; ++i) {
    r.step();
    if (i == 3) CHECK(r.e().pos.y == F(-0.25));
    if (i == 6) CHECK(r.e().pos.y == F(-2.5));
    if (r.has(EventType::Eliminated)) eliminated_at = i;
  }
  CHECK_EQ(eliminated_at, 54);
  CHECK(r.e().eliminated == 1);
  const Vec2 frozen = r.e().pos;
  r.step();
  CHECK(r.e().pos == frozen && r.ev.empty());  // eliminated entities no longer move
}

// Rising through a pass-through platform (top 10) and landing on it on the way
// down. From (0,5) with vy=3: y_k = 5 + 3k - k(k+1)/8, apex 21.5 at k=12, vy hits
// -1 at k=16 (y=19) then y = 35 - k, reaching exactly 10 at k=25.
void test_pass_through_from_below() {
  Runner r(config({solid(-100, 100, -10, 0), thin(-20, 20, 10)}), air(0, 5, 0, 3));
  for (int k = 1; k <= 24; ++k) {
    r.step();
    CHECK(r.e().mode == Airb);
    if (k == 2) CHECK(r.e().pos.y == F(10.25));  // already through the platform
    if (k == 12) CHECK(r.e().pos.y == F(21.5));
    if (k == 24) CHECK(r.e().pos.y == F(11));
  }
  r.step();
  CHECK(r.e().mode == Idle && r.e().pos.y == F(10) && r.e().support == 1 && r.has(EventType::Landed, 1));
}

// A fresh down press on a pass-through platform drops through it. The press frame
// also fast-falls: 8, 6, 4, 2, then 0 on the fifth frame, landing on the floor.
void test_drop_through() {
  Config c = config({solid(-100, 100, -10, 0), thin(-20, 20, 10)});
  Runner r(c, spawn_on(c, 1, F(0)));
  r.step();
  CHECK(r.e().mode == Idle && r.e().pos.y == F(10));
  r.step(Intent{F(0), false, true});
  CHECK(r.e().mode == Airb && r.e().pos.y == F(8) && r.e().support == -1 && r.has(EventType::Fell));
  for (int k = 1; k <= 3; ++k) r.step();
  CHECK(r.e().pos.y == F(2) && r.e().mode == Airb);
  r.step();
  CHECK(r.e().mode == Idle && r.e().pos.y == F(0) && r.e().support == 0 && r.has(EventType::Landed, 0));
}

// Holding down while landing on a pass-through platform does not drop: only a fresh press does.
void test_down_held_on_landing() {
  Runner r(config({thin(-20, 20, 10)}), air(0, 11.5, 0, -1));
  Intent down{F(0), false, true};
  r.step(down);  // fast-fall to -2: 9.5 -> lands on 10
  CHECK(r.e().mode == Idle && r.e().pos.y == F(10) && r.e().support == 0);
  r.step(down);
  r.step(down);
  CHECK(r.e().mode == Idle && r.e().pos.y == F(10));
  r.step();  // release, press again: now it drops
  r.step(down);
  CHECK(r.e().mode == Airb);
}

// Solid platforms are walls: running right from x=47 at 1.5: 1.625 (48.625), 1.75
// would reach 50.375, so the body (right edge x+1) stops at 49 with vel.x = 0 and stays.
void test_wall() {
  Config c = config({solid(-100, 100, -10, 0), solid(50, 54, 0, 30)});
  EntityState e = spawn_on(c, 0, F(47));
  e.mode = Mode::Run;
  e.facing = 1;
  e.vel.x = F(1.5);
  e.prev_dir = 1;
  Runner r(c, e);
  Intent run{F(1)};
  r.step(run);
  CHECK(r.e().pos.x == F(48.625));
  r.step(run);
  CHECK(r.e().pos.x == F(49) && r.e().vel.x == F(0) && r.e().mode == Mode::Run);
  r.step(run);
  CHECK(r.e().pos.x == F(49) && r.e().vel.x == F(0));
  // From the other side: running left from x=57 stops at 55 (left edge x-1 = 54).
  EntityState f = spawn_on(c, 0, F(57));
  f.mode = Mode::Run;
  f.facing = -1;
  f.vel.x = F(-1.5);
  f.prev_dir = -1;
  Runner l(c, f);
  Intent left{F(-1)};
  l.step(left);  // -1.625: 55.375
  l.step(left);  // -1.75: would be 53.625
  CHECK(l.e().pos.x == F(55) && l.e().vel.x == F(0));
}

// Landing on top of the wall (top 30) rather than the floor under it.
void test_land_on_wall_top() {
  Runner r(config({solid(-100, 100, -10, 0), solid(50, 54, 0, 30)}), air(52, 33, 0, -1));
  r.step();
  r.step();
  CHECK(r.e().mode == Airb && r.e().pos.y == F(31));
  r.step();
  CHECK(r.e().mode == Idle && r.e().pos.y == F(30) && r.e().support == 1 && r.has(EventType::Landed, 1));
}

// Jumping up into a solid ceiling (underside y=10, body 4 tall): from (0,3) with vy=3 the
// head reaches 9.75, then 12.25 > 10, so the feet stop at 6 and vy is zeroed.
void test_head_bump() {
  Runner r(config({solid(-100, 100, -10, 0), solid(-10, 10, 10, 14)}), air(0, 3, 0, 3));
  r.step();
  CHECK(r.e().pos.y == F(5.75) && r.e().vel.y == F(2.75));
  r.step();
  CHECK(r.e().pos.y == F(6) && r.e().vel.y == F(0) && r.e().mode == Airb);
  r.step();
  CHECK(r.e().pos.y == F(5.75) && r.e().vel.y == F(-0.25));
  // A pass-through ceiling does not bump.
  Runner t(config({thin(-10, 10, 10)}), air(0, 3, 0, 3));
  t.step();
  t.step();
  CHECK(t.e().pos.y == F(8.25) && t.e().vel.y == F(2.5));
}

// Placed inside a solid, an entity is pushed out the shallowest way.
void test_depenetration() {
  Config c = config({solid(-100, 100, -10, 0), solid(50, 54, 0, 30)});
  EntityState e = spawn_on(c, 0, F(50.5));  // body [49.5, 51.5] overlaps the wall by 1.5 on its left
  Runner r(c, e);
  r.step();
  CHECK(r.e().pos.x == F(49) && r.e().pos.y == F(0));
  // Feet sunk 1 into the floor: pushed up and landed.
  Runner s(c, air(0, -1, 0, 0));
  s.step();
  CHECK(s.e().pos.y == F(0) && s.e().mode == Idle && s.has(EventType::Landed, 0));
}

// A jump from the floor: the squat, the launch, a full hop and landing in the same place.
void test_jump_and_return() {
  Config c = config({solid(-100, 100, -10, 0)});
  Runner r(c, spawn_on(c, 0, F(0)));
  Intent hold{F(0), true};
  r.step(hold);
  r.step(hold);
  CHECK(r.e().mode == Mode::JumpSquat && r.e().support == 0 && r.e().pos.y == F(0));
  r.step(hold);
  CHECK(r.e().mode == Airb && r.e().pos.y == F(2) && r.e().support == -1);
  for (int k = 0; k < 19; ++k) r.step(hold);
  CHECK(r.e().mode == Idle && r.e().pos.y == F(0) && r.e().support == 0);  // see air_movement_test: lands on frame 22
  CHECK_EQ(r.steps, 22);
}

// Entities are independent: two entities behave exactly as each would alone.
void test_two_entities() {
  Config c = config({solid(-100, 100, -10, 0)}, 2);
  World w;
  w.entities = {air(-10, 2.5, 0, -1), air(10, 3.5, 0, -1)};
  Events ev;
  std::vector<Intent> in(2);
  for (int i = 0; i < 3; ++i) fp::step(c, w, in, ev);
  CHECK(w.entities[0].mode == Idle && w.entities[1].mode == Airb && w.entities[1].pos.y == F(0.5));
  CHECK_EQ(w.frame, 3);
  fp::step(c, w, in, ev);
  CHECK(w.entities[1].mode == Idle);
  CHECK(ev.size() == 1 && ev[0].type == EventType::Landed && ev[0].id == 1);
}

}  // namespace

int main() {
  test_landing();
  test_walk_off_and_eliminated();
  test_pass_through_from_below();
  test_drop_through();
  test_down_held_on_landing();
  test_wall();
  test_land_on_wall_top();
  test_head_bump();
  test_depenetration();
  test_jump_and_return();
  test_two_entities();
  return fp_test::finish();
}
