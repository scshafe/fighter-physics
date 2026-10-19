// Platform lifecycle: moving, crumbling, respawning, destroying. Hand-worked
// numbers, as in world_test.cpp.
#include "world_fixture.hpp"

using namespace fp;
using namespace fixture;

namespace {

const Mode Idle = Mode::Idle, Airb = Mode::Airborne;

bool has_event(const Events& ev, EventType t, int id) {
  for (const Event& e : ev)
    if (e.type == t && e.id == id) return true;
  return false;
}

PlatformDef crumbling(int shake, int respawn) {
  PlatformDef d = solid(-100, 100, -10, 0);
  d.shake_frames = shake;
  d.respawn_frames = respawn;
  return d;
}

// shake 3, respawn 4, entity stands on it from the start.
//   frame 1: it has been stood on: Shaking (timer 3)    frame 4: timer 0: Gone, the rider falls
//   frames 5-8: gone (timer 4 -> 0), the rider falls    frame 8: Solid again (Respawned)
// The faller is at y = -0.25, -0.75, -1.5, -2.5 on frames 5-8, i.e. inside the respawned
// platform, which pushes it up and lands it on frame 9.
void test_crumble_and_respawn() {
  Config c = config({crumbling(3, 4)});
  Runner r(c, spawn_on(c, 0, F(0)));
  r.step();
  CHECK(r.w.platforms[0].phase == PlatformPhase::Shaking && has_event(r.ev, EventType::PlatformShaking, 0));
  r.step();
  r.step();
  CHECK(r.w.platforms[0].phase == PlatformPhase::Shaking && r.e().mode == Idle && r.ev.empty());
  r.step();  // frame 4
  CHECK(r.w.platforms[0].phase == PlatformPhase::Gone && has_event(r.ev, EventType::PlatformCrumbled, 0));
  CHECK(r.e().mode == Airb && r.e().support == -1 && r.has(EventType::Fell) && r.e().pos.y == F(0));
  r.step();  // frame 5
  CHECK(r.e().pos.y == F(-0.25) && r.ev.empty());
  r.step();
  r.step();
  CHECK(r.e().pos.y == F(-1.5) && r.w.platforms[0].phase == PlatformPhase::Gone);
  r.step();  // frame 8
  CHECK(r.e().pos.y == F(-2.5));
  CHECK(r.w.platforms[0].phase == PlatformPhase::Solid && has_event(r.ev, EventType::PlatformRespawned, 0));
  r.step();  // frame 9: pushed out of the respawned platform and landed
  CHECK(r.e().mode == Idle && r.e().pos.y == F(0) && r.has(EventType::Landed, 0));
  // Standing on it again restarts the cycle.
  CHECK(r.w.platforms[0].phase == PlatformPhase::Shaking);
}

// A platform nobody stands on does not crumble; shake_frames = 0 never does;
// respawn_frames = 0 stays gone.
void test_crumble_conditions() {
  Config c = config({crumbling(3, 4), solid(200, 300, -10, 0)});
  Runner r(c, spawn_on(c, 1, F(250)));
  for (int i = 0; i < 20; ++i) r.step();
  CHECK(r.w.platforms[0].phase == PlatformPhase::Solid);
  CHECK(r.w.platforms[1].phase == PlatformPhase::Solid);

  Config never = config({crumbling(0, 4)});
  Runner n(never, spawn_on(never, 0, F(0)));
  for (int i = 0; i < 20; ++i) n.step();
  CHECK(n.w.platforms[0].phase == PlatformPhase::Solid && n.e().mode == Idle);

  Config perm = config({crumbling(1, 0)});
  Runner q(perm, spawn_on(perm, 0, F(0)));
  for (int i = 0; i < 30; ++i) q.step();
  CHECK(q.w.platforms[0].phase == PlatformPhase::Gone);
}

// Destroying a platform: riders fall, no respawn, events.
void test_destroy() {
  Config c = config({crumbling(3, 4)});
  Runner r(c, spawn_on(c, 0, F(0)));
  r.step();
  r.ev.clear();
  destroy_platform(c, r.w, 0, r.ev);
  CHECK(r.w.platforms[0].phase == PlatformPhase::Destroyed);
  CHECK(has_event(r.ev, EventType::PlatformDestroyed, 0) && r.has(EventType::Fell) && r.e().mode == Airb);
  for (int i = 0; i < 30; ++i) r.step();
  CHECK(r.w.platforms[0].phase == PlatformPhase::Destroyed);
  Events again;
  destroy_platform(c, r.w, 0, again);  // a second destroy is a no-op
  CHECK(again.empty());
  // Nothing lands on it any more.
  CHECK(r.e().mode == Airb && r.e().pos.y < F(-10));
}

// Horizontal travel (8, 0) over a 4-frame cycle: offsets 4, 8, 4, 0 at the ends of frames
// 1-4. The rider is carried by the same amounts and stays supported.
void test_moving_platform_carries_rider() {
  PlatformDef m = solid(-10, 10, -10, 0);
  m.travel = Vec2::from_double(8, 0);
  m.move_period = 4;
  Config c = config({m});
  Runner r(c, spawn_on(c, 0, F(0)));
  const double x[] = {4, 8, 4, 0, 4};
  for (double want : x) {
    r.step();
    CHECK(r.e().pos.x == F(want) && r.e().mode == Idle && r.e().support == 0);
  }
}

// Vertical travel (0, 8): the rider goes up and down with it.
void test_vertical_platform() {
  PlatformDef m = solid(-10, 10, -10, 0);
  m.travel = Vec2::from_double(0, 8);
  m.move_period = 4;
  Config c = config({m});
  Runner r(c, spawn_on(c, 0, F(0)));
  const double y[] = {4, 8, 4, 0};
  for (double want : y) {
    r.step();
    CHECK(r.e().pos.y == F(want) && r.e().mode == Idle && r.e().support == 0);
  }
}

// Contacts see the platform where the previous frame left it. Travel (8, 0), 4-frame cycle:
// offsets 0 (frame 1's contacts), 4 (frame 2), 8 (frame 3). A falling entity at x=14,
// 3 up, descends 1 a frame: it lands on frame 3, when the platform's right edge (10+8)
// is under it, and is then carried back by 4 (offset 8 -> 4): x = 10.
void test_land_on_moving_platform() {
  PlatformDef m = solid(-10, 10, -10, 0);
  m.travel = Vec2::from_double(8, 0);
  m.move_period = 4;
  Runner r(config({m}), air(14, 3, 0, -1));
  r.step();
  r.step();
  CHECK(r.e().mode == Airb && r.e().pos.y == F(1));
  r.step();
  CHECK(r.e().mode == Idle && r.e().pos.y == F(0) && r.e().support == 0 && r.e().pos.x == F(10));
}

// A solid that slides into an entity pushes it out the shallowest way. Travel 16 over a
// 4-frame cycle: the wall [-20, -10] is at [-12, -2] after frame 1 and [-4, 6] after frame 2.
// Frame 2: the body [-4, -2] overlaps by 2 on the wall's right side: x = -2 + 1 = -1.
// Frame 3: the body [-2, 0] is inside [-4, 6] by 4 on the left side (and 4 downward; left wins
// ties): x = -4 - 1 = -5. The way out is whichever is shallowest, not the wall's travel direction.
void test_platform_pushes_entity() {
  PlatformDef m = solid(-20, -10, 0, 10);
  m.travel = Vec2::from_double(16, 0);
  m.move_period = 4;
  Config c = config({solid(-100, 100, -10, 0), m});
  Runner r(c, spawn_on(c, 0, F(-3)));
  r.step();
  CHECK(r.e().pos.x == F(-3));
  r.step();
  CHECK(r.e().pos.x == F(-1) && r.e().mode == Idle);
  r.step();
  CHECK(r.e().pos.x == F(-5) && r.e().mode == Idle);
}

}  // namespace

int main() {
  test_crumble_and_respawn();
  test_crumble_conditions();
  test_destroy();
  test_moving_platform_carries_rider();
  test_vertical_platform();
  test_land_on_moving_platform();
  test_platform_pushes_entity();
  return fp_test::finish();
}
