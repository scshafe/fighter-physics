// Placeables: walls (so far). Hand-worked numbers, as in world_test.cpp. The floor is
// [-100, 100] x [-10, 0], so a wall's platform id is 1 + its slot.
#include "world_fixture.hpp"

using namespace fp;
using namespace fixture;

namespace {

Placeable wall(double l, double r, double b, double t, int life = 0) {
  Placeable p;
  p.rect = {F(l), F(r), F(b), F(t)};
  p.life = life;
  return p;
}

bool has_event(const Events& ev, EventType t, int id) {
  for (const Event& e : ev)
    if (e.type == t && e.id == id) return true;
  return false;
}

Config stage() { return config({solid(-100, 100, -10, 0)}); }

// Running at 2 a frame from x=-10 into a wall whose left side is at 10: the body's right edge
// (x + 1) first passes 10 at x=10 (frame 11), so it stops at x = 9 with no speed.
// Mirror image to the left.
void test_wall_blocks() {
  Config c = stage();
  EntityState e = spawn_on(c, 0, F(-10));
  e.mode = Mode::Run;
  e.vel.x = F(2);
  Runner r(c, e);
  CHECK_EQ(place(r.w, wall(10, 12, 0, 8)), 0);
  Intent go;
  go.move_x = F(1);
  for (int i = 0; i < 9; ++i) r.step(go);
  CHECK(r.e().pos.x == F(8) && r.e().vel.x == F(2));
  r.step(go);
  CHECK(r.e().pos.x == F(9) && r.e().vel.x == F(0));

  EntityState l = spawn_on(c, 0, F(30));
  l.mode = Mode::Run;
  l.vel.x = F(-2);
  l.facing = -1;
  Runner q(c, l);
  place(q.w, wall(10, 12, 0, 8));
  Intent back;
  back.move_x = F(-1);
  for (int i = 0; i < 20; ++i) q.step(back);
  CHECK(q.e().pos.x == F(13) && q.e().vel.x == F(0));
}

// Falling onto the top from 12 up at x=11: y = 11.75, 11.25, 10.5, 9.5, 8.5, then the sixth
// frame crosses the top (8) and lands on it. The wall is platform 1.
void test_land_on_wall() {
  Runner r(stage(), air(11, 12, 0, 0));
  place(r.w, wall(10, 12, 0, 8));
  for (int i = 0; i < 5; ++i) r.step();
  CHECK(r.e().pos.y == F(8.5) && r.e().mode == Mode::Airborne);
  r.step();
  CHECK(r.e().pos.y == F(8) && r.e().mode == Mode::Idle && r.e().support == 1);
  CHECK(r.has(EventType::Landed, 1));
}

// Rising into the underside: the head (4 above the feet) stops at the bottom (20).
void test_bump_underside() {
  Runner r(stage(), air(11, 10, 0, 3));
  place(r.w, wall(10, 12, 20, 24));
  for (int i = 0; i < 10; ++i) r.step();
  CHECK(r.e().pos.y + F(4) <= F(20) && r.e().vel.y <= F(0));
  CHECK(r.e().pos.y > F(10));
}

// A wall with a life of 3 frames: standing on it for two, gone on the third, the rider falls,
// the slot is free and reused.
void test_expiry() {
  Config c = stage();
  EntityState e = spawn_on(c, 0, F(11));
  e.pos.y = F(8);
  e.support = 1;
  Runner r(c, e);
  CHECK_EQ(place(r.w, wall(10, 12, 0, 8, 3)), 0);
  r.step();
  r.step();
  CHECK(r.e().mode == Mode::Idle && r.e().support == 1 && r.w.placeables[0].life == 1);
  r.step();
  CHECK(has_event(r.ev, EventType::PlaceableRemoved, 0) && r.has(EventType::Fell));
  CHECK(r.e().mode == Mode::Airborne && r.e().support == -1 && r.w.placeables[0].active == 0);
  CHECK_EQ(place(r.w, wall(50, 52, 0, 8)), 0);
}

// Slots: lowest free first, full at kMaxPlaceables, bad rects refused, removal frees a slot.
void test_slots() {
  Config c = stage();
  World w = make_world(c);
  CHECK_EQ(place(w, wall(1, 1, 0, 1)), -1);  // empty
  CHECK_EQ(place(w, wall(2, 1, 0, 1)), -1);  // inverted
  CHECK_EQ(place(w, wall(0, 1, 0, 1, -1)), -1);
  for (int i = 0; i < kMaxPlaceables; ++i) CHECK_EQ(place(w, wall(i, i + 1, 0, 1)), i);
  CHECK_EQ(place(w, wall(0, 1, 0, 1)), -1);
  Events ev;
  remove_placeable(c, w, 5, ev);
  CHECK(has_event(ev, EventType::PlaceableRemoved, 5) && w.placeables[5].active == 0);
  remove_placeable(c, w, 5, ev);  // already free: nothing more
  remove_placeable(c, w, 99, ev);
  CHECK_EQ(ev.size(), 1);
  CHECK_EQ(place(w, wall(7, 8, 0, 1)), 5);
}

// remove_placeable: riders fall.
void test_remove() {
  Config c = stage();
  EntityState e = spawn_on(c, 0, F(11));
  e.pos.y = F(8);
  e.support = 1;
  Runner r(c, e);
  place(r.w, wall(10, 12, 0, 8));
  r.step();
  CHECK(r.e().mode == Mode::Idle);
  r.ev.clear();
  remove_placeable(c, r.w, 0, r.ev);
  CHECK(has_event(r.ev, EventType::PlaceableRemoved, 0) && r.e().mode == Mode::Airborne && r.e().support == -1);
}

// A wall placed on top of an entity pushes it out the shallowest way (sideways here: the
// overlaps are 2.5 left, 2.5 right and 10 down; left wins ties).
void test_place_on_entity() {
  Config c = stage();
  Runner r(c, spawn_on(c, 0, F(0)));
  place(r.w, wall(-1.5, 1.5, 0, 10));
  r.step();
  CHECK(r.e().pos.x == F(-2.5) && r.e().mode == Mode::Idle);
}

}  // namespace

int main() {
  test_wall_blocks();
  test_land_on_wall();
  test_bump_underside();
  test_expiry();
  test_slots();
  test_remove();
  test_place_on_entity();
  return fp_test::finish();
}
