#pragma once
// Shared fixtures for the World tests: exactly representable parameters and a
// small runner that steps entity 0 and inspects the frame's events.

#include <utility>
#include <vector>

#include "check.hpp"
#include "fp/world.hpp"

namespace fixture {

using namespace fp;


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
  Runner(Config c, EntityState e) : cfg(std::move(c)), w(make_world(cfg)) { w.entities.push_back(e); }
  const EntityState& e() const { return w.entities[0]; }
  bool step(Intent in = {}) {
    std::vector<Intent> v(w.entities.size(), in);
    ev.clear();
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

}  // namespace fixture
