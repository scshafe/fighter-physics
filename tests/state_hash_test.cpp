// World hash: stable, and sensitive to every field of the state.
#include <cstdio>
#include <functional>
#include <utility>
#include <vector>

#include "check.hpp"
#include "fp/state.hpp"

using namespace fp;

namespace {

World sample() {
  World w;
  w.frame = 7;
  w.platforms.resize(2);
  w.platforms[0].phase = PlatformPhase::Shaking;
  w.platforms[0].timer = 5;
  w.platforms[0].clock = 3;
  w.platforms[0].offset = Vec2::from_double(1.5, -2.25);
  w.entities.resize(2);
  EntityState& e = w.entities[1];
  e.pos = Vec2::from_double(10.5, 20.25);
  e.vel = Vec2::from_double(-0.5, 1.75);
  e.mode = Mode::Airborne;
  e.facing = -1;
  e.prev_dir = -1;
  e.flick_age = 4;
  e.frames = 9;
  e.jumps_used = 1;
  e.prev_jump = 1;
  e.fast_falling = 1;
  e.prev_down = 1;
  e.eliminated = 0;
  e.support = 1;
  e.support_entity = 0;
  e.drop_timer = 2;
  return w;
}

void test_stable() {
  CHECK(hash(sample()) == hash(sample()));
  // A pinned value: the hash must not change with the platform, compiler or
  // library (CI runs this on Linux and macOS). If the state layout changes on
  // purpose, update it deliberately.
  CHECK(hash(sample()) == 0xf7f9f7783b8cb01dULL);
}

void test_sensitive() {
  const std::uint64_t base = hash(sample());
  const std::vector<std::function<void(World&)>> edits = {
      [](World& w) { w.frame += 1; },
      [](World& w) { w.platforms[0].phase = PlatformPhase::Gone; },
      [](World& w) { w.platforms[0].timer += 1; },
      [](World& w) { w.platforms[0].clock += 1; },
      [](World& w) { w.platforms[0].offset.x = Fixed::from_raw(w.platforms[0].offset.x.raw() + 1); },
      [](World& w) { w.platforms[0].offset.y = Fixed::from_raw(w.platforms[0].offset.y.raw() + 1); },
      [](World& w) { w.platforms.pop_back(); },
      [](World& w) { w.entities.pop_back(); },
      [](World& w) { w.entities[1].pos.x = Fixed::from_raw(w.entities[1].pos.x.raw() + 1); },
      [](World& w) { w.entities[1].pos.y = Fixed::from_raw(w.entities[1].pos.y.raw() + 1); },
      [](World& w) { w.entities[1].vel.x = Fixed::from_raw(w.entities[1].vel.x.raw() + 1); },
      [](World& w) { w.entities[1].vel.y = Fixed::from_raw(w.entities[1].vel.y.raw() + 1); },
      [](World& w) { w.entities[1].mode = Mode::Run; },
      [](World& w) { w.entities[1].facing = 1; },
      [](World& w) { w.entities[1].prev_dir = 0; },
      [](World& w) { w.entities[1].flick_age += 1; },
      [](World& w) { w.entities[1].frames += 1; },
      [](World& w) { w.entities[1].jumps_used = 0; },
      [](World& w) { w.entities[1].prev_jump = 0; },
      [](World& w) { w.entities[1].fast_falling = 0; },
      [](World& w) { w.entities[1].prev_down = 0; },
      [](World& w) { w.entities[1].eliminated = 1; },
      [](World& w) { w.entities[1].support = 0; },
      [](World& w) { w.entities[1].support_entity = 1; },
      [](World& w) { w.entities[1].drop_timer = 0; },
      [](World& w) { w.entities[0].frames += 1; },  // the other entity counts too
  };
  int i = 0;
  for (const auto& edit : edits) {
    World w = sample();
    edit(w);
    if (hash(w) == base) {
      std::fprintf(stderr, "edit %d did not change the hash\n", i);
      ++fp_test::failures();
    }
    ++i;
  }
  // Order matters: swapping two different entities changes the hash.
  World a = sample(), b = sample();
  std::swap(b.entities[0], b.entities[1]);
  CHECK(hash(a) != hash(b));
}

}  // namespace

int main() {
  test_stable();
  test_sensitive();
  return fp_test::finish();
}
