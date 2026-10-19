#pragma once
// A busy, deterministic scenario shared by the determinism and rollback tests
// and the benchmark: entities of three archetypes dropped onto a stage with a
// floor, a pass-through shelf, a crumbling platform, a moving platform and a
// wall. Several share an x, so they stack. The "game" respawns eliminated
// entities at the top. Intents are random but a pure function of the seed.

#include <cstdint>
#include <vector>

#include "check.hpp"
#include "fp/jobs.hpp"
#include "fp/state.hpp"
#include "fp/world.hpp"

namespace scenario {

using namespace fp;

inline Fixed F(double v) { return Fixed::from_double(v); }

inline Config make_config(int entities) {
  Config c;
  auto solid = [](double l, double r, double b, double t) {
    PlatformDef d;
    d.rect = {F(l), F(r), F(b), F(t)};
    return d;
  };
  PlatformDef floor = solid(-60, 60, -10, 0);
  PlatformDef shelf = solid(-30, 30, 20, 21);
  shelf.kind = PlatformKind::PassThrough;
  PlatformDef crumble = solid(-90, -62, 10, 12);
  crumble.shake_frames = 25;
  crumble.respawn_frames = 40;
  PlatformDef mover = solid(62, 90, 10, 12);
  mover.travel = Vec2::from_double(0, 30);
  mover.move_period = 120;
  PlatformDef wall = solid(100, 104, 0, 40);
  PlatformDef far_floor = solid(94, 140, -10, 0);  // crumbles by stress
  far_floor.respawn_frames = 40;
  far_floor.stress = {F(0.2), F(0.5), F(0.02), F(0.01), F(0.5), F(0.5), F(0.01), F(0.005)};
  c.stage.platforms = {floor, shelf, crumble, mover, wall, far_floor};
  c.stage.blast = {F(-140), F(150), F(-40), F(120)};
  c.params.assign(entities, MovementParams{});
  for (int i = 0; i < entities; ++i) {  // a few different archetypes
    MovementParams& p = c.params[i];
    if (i % 3 == 1) {
      p.run_max = F(2.2);
      p.full_hop_vel = F(3.0);
      p.body_half_width = F(3.0);
    } else if (i % 3 == 2) {
      p.gravity = F(0.12);
      p.aerial_jumps = 2;
    }
  }
  return c;
}

inline EntityState spawn(int id) {
  // Spread over the platforms, several at the same x so they stack on landing.
  EntityState e;
  e.mode = Mode::Airborne;
  e.pos = Vec2::from_double(-50.0 + 10.0 * (id % 8) + (id % 5 == 0 ? 0.5 : 0.0), 30.0 + 7.0 * (id / 8));
  return e;
}

class Scenario {
 public:
  explicit Scenario(int entities, std::uint64_t seed = 0xf16417e5ULL)
      : cfg(make_config(entities)), w(make_world(cfg)), rng_(seed), intents_(entities) {
    for (int i = 0; i < entities; ++i) w.entities.push_back(spawn(i));
  }

  // The next frame's inputs (advances the random stream; intents stick a little).
  const std::vector<Intent>& next_intents() {
    for (Intent& x : intents_) {
      const std::uint64_t r = rng_.next();
      if ((r & 7) == 0) x.move_x = F(static_cast<int>((r >> 8) % 5) * 0.5 - 1.0);
      if (((r >> 16) & 7) == 0) x.jump = !x.jump;
      x.fast_fall = ((r >> 24) & 15) == 0;
    }
    return intents_;
  }

  // The game's placements (walls, and ice and mud on the floor): a function of the frame, so replays from a snapshot repeat them.
  void place_scheduled() {
    if (w.frame % 60 == 20) {
      const double x = -40.0 + 4.0 * static_cast<double>((w.frame / 60) % 10);
      Placeable wall;
      wall.rect = {F(x), F(x + 3), F(0), F(6)};
      wall.life = 90;
      place(w, wall);
    }
    if (w.frame % 90 == 5) {
      Placeable patch;
      const bool ice = (w.frame / 90) % 2 == 0;
      patch.kind = ice ? PlaceableKind::Slick : PlaceableKind::Sticky;
      patch.rect = {F(-50), F(10), F(-1), F(0)};
      patch.life = 120;
      patch.traction_scale = ice ? F(0.25) : F(3);
      patch.speed_scale = ice ? F(1) : F(0.5);
      place(w, patch);
    }
  }

  // One frame of the simulation as the game would run it: step, then bring
  // eliminated entities back. A pure function of the world and the intents.
  void advance(const std::vector<Intent>& in, JobSystem* jobs) {
    ev.clear();
    place_scheduled();
    step(cfg, w, in, ev, jobs);
    for (EntityState& e : w.entities) {
      if (e.eliminated != 0) {
        const int id = static_cast<int>(&e - w.entities.data());
        e = spawn(id);
        e.pos.y = F(60);
        ++respawns;
      }
    }
  }

  Config cfg;
  World w;
  Events ev;
  int respawns = 0;

 private:
  fp_test::Rng rng_;
  std::vector<Intent> intents_;
};

}  // namespace scenario
