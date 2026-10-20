// Per-entity movement modifiers (EntityState::mod). Hand-worked numbers, as in world_test.cpp:
// run_max 2, run_accel 0.125, traction 0.0625, walk_init 0.25, walk_accel 0.125, full hop 2,
// air_max 1.
#include "world_fixture.hpp"

using namespace fp;
using namespace fixture;

namespace {

Config stage() { return config({solid(-1000, 1000, -10, 0)}); }

Modifier mod(int frames) {
  Modifier m;
  m.frames = frames;
  return m;
}

// Running at 2 with the ground speed halved for 3 frames: the cap is 1 and the run sheds the
// traction a frame (1.9375, 1.875, 1.8125); then the modifier is gone, the cap is 2 again and the
// run accelerates (+0.125 to 1.9375). Applied for exactly 3 steps; the state is then the default.
void test_ground_speed_and_expiry() {
  Config c = stage();
  EntityState e = spawn_on(c, 0, F(-100));
  e.mode = Mode::Run;
  e.vel.x = F(2);
  e.mod = mod(3);
  e.mod.ground_speed = F(0.5);
  Runner r(c, e);
  Intent go;
  go.move_x = F(1);
  r.step(go);
  CHECK(r.e().vel.x == F(1.9375) && r.e().mod.frames == 2);
  r.step(go);
  r.step(go);
  CHECK(r.e().vel.x == F(1.8125) && r.e().mod.frames == 0 && r.e().mod.ground_speed == Fixed::one());
  r.step(go);
  CHECK(r.e().vel.x == F(1.9375));
  // Setting it every tick keeps it going.
  for (int i = 0; i < 40; ++i) {
    r.w.entities[0].mod = mod(1);
    r.w.entities[0].mod.ground_speed = F(0.5);
    r.step(go);
  }
  CHECK(r.e().vel.x == F(1));
}

// Walking at half tilt (cap 0.5): the first frame sets 0.25, then +0.125 a frame; with the ground
// acceleration halved +0.0625: 0.25, 0.3125, 0.375, 0.4375, 0.5.
void test_ground_accel() {
  Config c = stage();
  EntityState e = spawn_on(c, 0, F(0));
  e.mod = mod(10);
  e.mod.ground_accel = F(0.5);
  Runner r(c, e);
  Intent walk;
  walk.move_x = F(0.5);
  r.step(walk);
  CHECK(r.e().vel.x == F(0.25));
  r.step(walk);
  CHECK(r.e().vel.x == F(0.3125));
  r.step(walk);
  r.step(walk);
  r.step(walk);
  CHECK(r.e().vel.x == F(0.5));
}

// Air speed and the jump: pushing right in the air reaches air_max (1) normally and 0.5 with the
// air speed halved; the same hop takes off 1.0 slower (2 * 0.5 fewer) with the jump halved.
void test_air_and_jump() {
  Config c = stage();
  {
    Runner a(c, air(0, 50, 0, 0)), b(c, air(0, 50, 0, 0));
    b.w.entities[0].mod = mod(100);
    b.w.entities[0].mod.air_speed = F(0.5);
    Intent right;
    right.move_x = F(1);
    for (int i = 0; i < 40; ++i) {
      a.step(right);
      b.step(right);
    }
    CHECK(a.e().vel.x == F(1) && b.e().vel.x == F(0.5));
  }
  {
    Runner a(c, spawn_on(c, 0, F(0))), b(c, spawn_on(c, 0, F(0)));
    b.w.entities[0].mod = mod(100);
    b.w.entities[0].mod.jump = F(0.5);
    Intent hop;
    hop.jump = true;
    for (int i = 0; i < 4; ++i) {  // squat 3 frames, then airborne
      a.step(hop);
      b.step(hop);
    }
    CHECK(a.e().mode == Mode::Airborne && b.e().mode == Mode::Airborne);
    CHECK(a.e().vel.y - b.e().vel.y == F(1));
  }
}

// A patch multiplies with the modifier: ice halving the traction and a modifier halving the cap of a
// run at 2 (shedding 0.0625 * 0.5 a frame instead of 0.0625).
void test_with_patch() {
  Config c = stage();
  EntityState e = spawn_on(c, 0, F(-100));
  e.mode = Mode::Run;
  e.vel.x = F(2);
  e.mod = mod(10);
  e.mod.ground_speed = F(0.5);
  Runner r(c, e);
  Placeable ice;
  ice.kind = PlaceableKind::Slick;
  ice.rect = {F(-200), F(200), F(-1), F(0)};
  ice.traction_scale = F(0.5);
  place(r.w, ice);
  Intent go;
  go.move_x = F(1);
  r.step(go);
  CHECK(r.e().vel.x == F(1.96875));
}

}  // namespace

int main() {
  test_ground_speed_and_expiry();
  test_ground_accel();
  test_air_and_jump();
  test_with_patch();
  return fp_test::finish();
}
