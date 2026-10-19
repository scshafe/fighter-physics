// Crumbling by accumulated stress (PlatformDef::stress). Hand-worked numbers, as in
// platform_test.cpp: shake_at 0.25, doom_at 0.5, still 0.125 a frame, moving 0.0625,
// still below speed 0.5, rider share 0.5, doom 0.0625, decay 0.0625, respawn 4.
#include "world_fixture.hpp"

using namespace fp;
using namespace fixture;

namespace {

bool has_event(const Events& ev, EventType t, int id) {
  for (const Event& e : ev)
    if (e.type == t && e.id == id) return true;
  return false;
}

PlatformDef crumbler(double l, double r) {
  PlatformDef d = solid(l, r, -10, 0);
  d.respawn_frames = 4;
  d.stress = {F(0.25), F(0.5), F(0.125), F(0.0625), F(0.5), F(0.5), F(0.0625), F(0.0625)};
  return d;
}

// Standing still: +0.125 a frame. Frame 2: 0.25 Shaking. Frame 4: 0.5 Doomed. Doomed adds
// the doom rate too (0.1875 a frame): frame 5 0.6875, frame 6 0.875, frame 7 1.0625 -> Gone.
// The platform stays solid until then; the stress restarts from 0; it respawns 4 frames later.
void test_standing_still() {
  Config c = config({crumbler(-100, 100)});
  Runner r(c, spawn_on(c, 0, F(0)));
  r.step();
  CHECK(r.w.platforms[0].stress == F(0.125) && r.w.platforms[0].phase == PlatformPhase::Solid && r.ev.empty());
  r.step();
  CHECK(r.w.platforms[0].stress == F(0.25) && r.w.platforms[0].phase == PlatformPhase::Shaking);
  CHECK(has_event(r.ev, EventType::PlatformShaking, 0));
  r.step();
  CHECK(r.w.platforms[0].stress == F(0.375) && r.ev.empty());
  r.step();
  CHECK(r.w.platforms[0].stress == F(0.5) && r.w.platforms[0].phase == PlatformPhase::Doomed);
  CHECK(has_event(r.ev, EventType::PlatformDoomed, 0));
  r.step();
  CHECK(r.w.platforms[0].stress == F(0.6875) && r.e().mode == Mode::Idle);
  r.step();
  CHECK(r.w.platforms[0].stress == F(0.875) && r.e().support == 0);
  r.step();  // frame 7
  CHECK(r.w.platforms[0].phase == PlatformPhase::Gone && r.w.platforms[0].stress == F(0));
  CHECK(has_event(r.ev, EventType::PlatformCrumbled, 0) && r.has(EventType::Fell) && r.e().mode == Mode::Airborne);
  for (int i = 0; i < 3; ++i) r.step();  // frames 8-10
  CHECK(r.w.platforms[0].phase == PlatformPhase::Gone);
  r.step();  // frame 11
  CHECK(r.w.platforms[0].phase == PlatformPhase::Solid && has_event(r.ev, EventType::PlatformRespawned, 0));
  CHECK(r.w.platforms[0].stress == F(0));
}

// Running at speed 2 (>= 0.5): 0.0625 a frame, so frame 4 reaches 0.25.
void test_moving_is_slower() {
  Config c = config({crumbler(-100, 100)});
  EntityState e = spawn_on(c, 0, F(-50));
  e.mode = Mode::Run;
  e.vel.x = F(2);
  e.facing = 1;
  Runner r(c, e);
  Intent go;
  go.move_x = F(1);
  for (int i = 0; i < 3; ++i) r.step(go);
  CHECK(r.e().mode == Mode::Run && r.e().vel.x == F(2));
  CHECK(r.w.platforms[0].stress == F(0.1875) && r.w.platforms[0].phase == PlatformPhase::Solid);
  r.step(go);
  CHECK(r.w.platforms[0].stress == F(0.25) && r.w.platforms[0].phase == PlatformPhase::Shaking);
}

// Nobody on it: the stress decays by 0.0625 a frame, and below shake_at it is Solid again.
// A doomed platform does not decay.
void test_decay() {
  Config c = config({crumbler(-100, 100)});
  Runner r(c, spawn_on(c, 0, F(0)));
  r.step();
  r.step();  // 0.25, Shaking
  r.w.entities[0] = air(0, 50, 0, 0);
  r.step();
  CHECK(r.w.platforms[0].stress == F(0.1875) && r.w.platforms[0].phase == PlatformPhase::Solid && r.ev.empty());
  r.step();
  r.step();
  r.step();
  r.step();
  CHECK(r.w.platforms[0].stress == F(0));  // floors at 0

  Runner d(c, spawn_on(c, 0, F(0)));
  for (int i = 0; i < 4; ++i) d.step();  // 0.5, Doomed
  d.w.entities[0] = air(0, 50, 0, 0);
  d.step();
  CHECK(d.w.platforms[0].stress == F(0.5625) && d.w.platforms[0].phase == PlatformPhase::Doomed);
}

// Feet on two platforms: each gets half the load (0.0625 on frame 1).
void test_straddle() {
  Config c = config({crumbler(-100, 0), crumbler(0, 100)});
  Runner r(c, spawn_on(c, 0, F(0)));
  r.step();
  CHECK(r.w.platforms[0].stress == F(0.0625) && r.w.platforms[1].stress == F(0.0625));
  // Fully on one: the other gets nothing.
  Runner s(c, spawn_on(c, 1, F(50)));
  s.step();
  CHECK(s.w.platforms[0].stress == F(0) && s.w.platforms[1].stress == F(0.125));
}

// An entity standing on another's head: the platform gets 0.125 from the one on it and a
// share (0.5) of 0.125 from the one above it, and 0.25 share^2 of the third.
void test_rider_share() {
  Config c = config({crumbler(-100, 100)}, 3);
  Runner r(c, spawn_on(c, 0, F(0)));
  for (int i = 1; i < 3; ++i) {
    EntityState up;
    up.pos = {F(0), F(4.0 * i)};
    up.support_entity = static_cast<std::int16_t>(i - 1);
    r.w.entities.push_back(up);
  }
  r.step();
  CHECK(r.w.entities[1].support_entity == 0 && r.w.entities[2].support_entity == 1);
  CHECK(r.w.platforms[0].stress == F(0.125 + 0.0625 + 0.03125));
}

// add_stress: one-off increments apply the phase changes at once.
void test_add_stress() {
  Config c = config({crumbler(-100, 100), solid(200, 300, -10, 0)});
  Runner r(c, spawn_on(c, 0, F(0)));
  Events ev;
  add_stress(c, r.w, 0, F(0.25), ev);
  CHECK(r.w.platforms[0].phase == PlatformPhase::Shaking && has_event(ev, EventType::PlatformShaking, 0));
  add_stress(c, r.w, 0, F(-0.125), ev);
  CHECK(r.w.platforms[0].stress == F(0.125) && r.w.platforms[0].phase == PlatformPhase::Solid);
  add_stress(c, r.w, 0, F(-0.5), ev);
  CHECK(r.w.platforms[0].stress == F(0));
  add_stress(c, r.w, 0, F(0.5), ev);
  CHECK(r.w.platforms[0].phase == PlatformPhase::Doomed && has_event(ev, EventType::PlatformDoomed, 0));
  ev.clear();
  add_stress(c, r.w, 1, F(0.5), ev);  // no stress model: nothing
  CHECK(ev.empty() && r.w.platforms[1].stress == F(0));
  add_stress(c, r.w, 0, F(5), ev);  // clamps at 1 and crumbles now
  CHECK(r.w.platforms[0].phase == PlatformPhase::Gone && has_event(ev, EventType::PlatformCrumbled, 0));
  CHECK(has_event(ev, EventType::Fell, 0) && r.e().mode == Mode::Airborne);
  ev.clear();
  add_stress(c, r.w, 0, F(1), ev);  // Gone: nothing
  CHECK(ev.empty());
}

}  // namespace

int main() {
  test_standing_still();
  test_moving_is_slower();
  test_decay();
  test_straddle();
  test_rider_share();
  test_add_stress();
  return fp_test::finish();
}
