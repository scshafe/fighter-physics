// Vertical-only body contact between entities (landing on a head, bumping an
// underside, riding) and pass-through at the sides. Hand-worked numbers.
#include "world_fixture.hpp"

using namespace fp;
using namespace fixture;

namespace {

const Mode Idle = Mode::Idle, Airb = Mode::Airborne;

struct Multi {
  Config cfg;
  World w;
  Events ev;
  Multi(Config c, std::vector<EntityState> es) : cfg(std::move(c)), w(make_world(cfg)) { w.entities = std::move(es); }
  EntityState& e(int i) { return w.entities[i]; }
  void step(std::vector<Intent> in = {}) {
    in.resize(w.entities.size());
    ev.clear();
    fp::step(cfg, w, in, ev);
  }
  bool has(EventType t, int id, int other) const {
    for (const Event& x : ev)
      if (x.type == t && x.id == id && x.other == other) return true;
    return false;
  }
};

EntityState riding(double x, double y, int under) {
  EntityState e;
  e.pos = Vec2::from_double(x, y);
  e.support_entity = static_cast<std::int16_t>(under);
  return e;
}

// B (id 0) stands on the floor; its head is at y=4. A (id 1) falls onto it from (0.5, 6.5) at 1 a frame:
// 5.5, 4.5, then 3.5 crosses the head: A lands at y=4.
void test_land_on_head() {
  Config c = config({solid(-100, 100, -10, 0)}, 2);
  Multi m(c, {spawn_on(c, 0, F(0)), air(0.5, 6.5, 0, -1)});
  m.step();
  m.step();
  CHECK(m.e(1).mode == Airb && m.e(1).pos.y == F(4.5));
  m.step();
  CHECK(m.e(1).mode == Idle && m.e(1).pos.y == F(4) && m.e(1).support_entity == 0 && m.e(1).support == -1);
  CHECK(m.has(EventType::LandedOnEntity, 1, 0));
  m.step();
  CHECK(m.e(1).pos.y == F(4) && m.e(1).mode == Idle && m.ev.empty());
}

// Beside each other, bodies do not interact: A dashes left through B (1.5, 1.625, 1.75 a frame
// from x=3 to -1.875), and neither is blocked or moved.
void test_pass_through_sideways() {
  Config c = config({solid(-100, 100, -10, 0)}, 2);
  Multi m(c, {spawn_on(c, 0, F(0)), spawn_on(c, 0, F(3))});
  for (int i = 0; i < 3; ++i) m.step({Intent{}, Intent{F(-1)}});
  CHECK(m.e(1).pos.x == F(-1.875) && m.e(1).mode == Mode::Dash);
  CHECK(m.e(0).pos.x == F(0) && m.e(0).mode == Idle && m.ev.empty());
}

// A rises into the underside of B, who stands on a pass-through platform at y=10: A's head (4 above its
// feet) is at 9.75 after the first frame and would be at 12.25: its feet stop at 10 - 4 = 6 and vy is zeroed.
void test_bump_from_below() {
  Config c = config({solid(-100, 100, -10, 0), thin(-20, 20, 10)}, 2);
  Multi m(c, {spawn_on(c, 1, F(0)), air(0, 3, 0, 3)});
  m.step();
  CHECK(m.e(1).pos.y == F(5.75));
  m.step();
  CHECK(m.e(1).pos.y == F(6) && m.e(1).vel.y == F(0) && m.e(1).mode == Airb && m.has(EventType::BumpedEntity, 1, 0));
  m.step();
  CHECK(m.e(1).pos.y == F(5.75) && m.e(1).vel.y == F(-0.25));
}

// A rides B; B walks right at half tilt: x = 0.25, 0.625, 1.125, 1.625, 2.125. The bodies
// (half width 1 each) overlap while |dx| < 2, so A keeps riding through frame 4, then falls.
void test_rider_loses_carrier() {
  Config c = config({solid(-100, 100, -10, 0)}, 2);
  Multi m(c, {spawn_on(c, 0, F(0)), riding(0, 4, 0)});
  for (int k = 1; k <= 4; ++k) {
    m.step({Intent{F(0.5)}, Intent{}});
    CHECK(m.e(1).mode == Idle && m.e(1).pos.y == F(4) && m.e(1).support_entity == 0);
  }
  CHECK(m.e(0).pos.x == F(1.625));
  m.step({Intent{F(0.5)}, Intent{}});
  CHECK(m.e(0).pos.x == F(2.125));
  CHECK(m.e(1).mode == Airb && m.e(1).support_entity == -1 && m.e(1).pos.y == F(4) && m.has(EventType::Fell, 1, -1));
  m.step({Intent{F(0.5)}, Intent{}});
  CHECK(m.e(1).pos.y == F(3.75));
}

// When B jumps (squat 3, launch at 2.0 on frame 3, then 1.75), A rides it: y = B.y + 4.
void test_rider_follows_jump() {
  Config c = config({solid(-100, 100, -10, 0)}, 2);
  Multi m(c, {spawn_on(c, 0, F(0)), riding(0, 4, 0)});
  Intent hold{F(0), true};
  m.step({hold, Intent{}});
  m.step({hold, Intent{}});
  CHECK(m.e(1).pos.y == F(4));
  m.step({hold, Intent{}});
  CHECK(m.e(0).pos.y == F(2) && m.e(1).pos.y == F(6) && m.e(1).mode == Idle);
  m.step({hold, Intent{}});
  CHECK(m.e(0).pos.y == F(3.75) && m.e(1).pos.y == F(7.75));
}

// A stack of three (C on the floor, B on C, A on B) rises together when C jumps, whatever the id order.
void test_stack() {
  Config c = config({solid(-100, 100, -10, 0)}, 3);
  const int orders[2][3] = {{0, 1, 2}, {2, 1, 0}};  // ids of {bottom, middle, top}
  for (const auto& o : orders) {
    std::vector<EntityState> es(3);
    es[o[0]] = spawn_on(c, 0, F(0));
    es[o[1]] = riding(0, 4, o[0]);
    es[o[2]] = riding(0, 8, o[1]);
    Multi m(c, es);
    std::vector<Intent> in(3);
    in[o[0]] = Intent{F(0), true};
    m.step(in);
    m.step(in);
    m.step(in);
    CHECK(m.e(o[0]).pos.y == F(2) && m.e(o[1]).pos.y == F(6) && m.e(o[2]).pos.y == F(10));
  }
}

// A rider falls when its carrier is eliminated.
void test_carrier_eliminated() {
  Config c = config({solid(-100, 100, -10, 0)}, 2);
  Multi m(c, {spawn_on(c, 0, F(0)), riding(0, 4, 0)});
  m.e(0).eliminated = 1;
  m.step();
  CHECK(m.e(1).mode == Airb && m.e(1).support_entity == -1 && m.has(EventType::Fell, 1, -1));
}

// A moving platform carries its rider and the rider's rider: travel (8, 0), 4-frame cycle: +4 after frame 1.
void test_platform_carries_stack() {
  PlatformDef d = solid(-10, 10, -10, 0);
  d.travel = Vec2::from_double(8, 0);
  d.move_period = 4;
  Config c = config({d}, 2);
  Multi m(c, {spawn_on(c, 0, F(0)), riding(0, 4, 0)});
  m.step();
  CHECK(m.e(0).pos.x == F(4) && m.e(1).pos.x == F(4) && m.e(1).support_entity == 0 && m.e(1).mode == Idle);
  m.step();
  CHECK(m.e(0).pos.x == F(8) && m.e(1).pos.x == F(8) && m.e(1).support_entity == 0);
}

// Results do not depend on which entity has which id: the same landing and the same bump with
// the ids swapped.
void test_id_order_independent() {
  Config c = config({solid(-100, 100, -10, 0), thin(-20, 20, 10)}, 2);
  for (int swap = 0; swap < 2; ++swap) {
    const int b = swap, a = 1 - swap;  // b: the one stood on, a: the faller
    std::vector<EntityState> es(2);
    es[b] = spawn_on(c, 0, F(0));
    es[a] = air(0.5, 6.5, 0, -1);
    Multi m(c, es);
    m.step();
    m.step();
    m.step();
    CHECK(m.e(a).mode == Idle && m.e(a).pos.y == F(4) && m.e(a).support_entity == b);

    std::vector<EntityState> up(2);
    up[b] = spawn_on(c, 1, F(0));
    up[a] = air(0, 3, 0, 3);
    Multi k(c, up);
    k.step();
    k.step();
    CHECK(k.e(a).pos.y == F(6) && k.e(a).vel.y == F(0) && k.has(EventType::BumpedEntity, a, b));
  }
}

}  // namespace

int main() {
  test_id_order_independent();
  test_land_on_head();
  test_pass_through_sideways();
  test_bump_from_below();
  test_rider_loses_carrier();
  test_rider_follows_jump();
  test_stack();
  test_carrier_eliminated();
  test_platform_carries_stack();
  return fp_test::finish();
}
