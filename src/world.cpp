#include "fp/world.hpp"

namespace fp {
namespace {

bool overlaps_x(const EntityState& e, const MovementParams& p, const Rect& r) noexcept {
  return e.pos.x + p.body_half_width > r.left && e.pos.x - p.body_half_width < r.right;
}

bool grounded_mode(Mode m) noexcept { return m <= Mode::Turnaround || m == Mode::JumpSquat; }

// Resolve one entity's contacts with the platforms after `advance` moved it
// from `prev`. Platforms are static during contact resolution (they move at
// the end of the frame), so every entity's result depends only on its own
// motion and the stage.
void resolve_platforms(const Stage& stage, EntityState& e, const MovementParams& p, Vec2 prev, int id,
                       Events& events) {
  const int n = static_cast<int>(stage.platforms.size());
  const Fixed hw = p.body_half_width, h = p.body_height;

  // Horizontal: solid platforms are walls (and the sides of floors).
  for (int i = 0; i < n; ++i) {
    const PlatformDef& d = stage.platforms[i];
    if (d.kind != PlatformKind::Solid) continue;
    const Rect& r = d.rect;
    if (!(prev.y + h > r.bottom && prev.y < r.top)) continue;  // not level with it
    if (prev.x + hw <= r.left && e.pos.x + hw > r.left) {
      e.pos.x = r.left - hw;
      e.vel.x = Fixed::zero();
    } else if (prev.x - hw >= r.right && e.pos.x - hw < r.right) {
      e.pos.x = r.right + hw;
      e.vel.x = Fixed::zero();
    }
  }

  // Vertical: land on the highest top crossed this frame, or bump the lowest
  // underside crossed.
  int landed = -1;
  if (e.mode == Mode::Airborne) {
    if (e.pos.y <= prev.y) {
      Fixed best{};
      for (int i = 0; i < n; ++i) {
        const PlatformDef& d = stage.platforms[i];
        if (d.kind == PlatformKind::PassThrough && e.drop_timer > 0) continue;
        const Rect& r = d.rect;
        if (prev.y >= r.top && e.pos.y <= r.top && overlaps_x(e, p, r) && (landed < 0 || r.top > best)) {
          landed = i;
          best = r.top;
        }
      }
    } else {
      int bump = -1;
      Fixed best{};
      for (int i = 0; i < n; ++i) {
        const PlatformDef& d = stage.platforms[i];
        if (d.kind != PlatformKind::Solid) continue;
        const Rect& r = d.rect;
        if (prev.y + h <= r.bottom && e.pos.y + h > r.bottom && overlaps_x(e, p, r) && (bump < 0 || r.bottom < best)) {
          bump = i;
          best = r.bottom;
        }
      }
      if (bump >= 0) {
        e.pos.y = best - h;
        e.vel.y = Fixed::zero();
      }
    }
  }
  if (landed >= 0) {
    e.pos.y = stage.platforms[landed].rect.top;
    land(e);
    e.support = static_cast<std::int16_t>(landed);
    events.push_back({EventType::Landed, id, landed});
  }

  // Still inside a solid (it moved or respawned onto the entity, or the entity
  // was placed there): push out along the shallowest way.
  for (int i = 0; i < n; ++i) {
    const PlatformDef& d = stage.platforms[i];
    if (d.kind != PlatformKind::Solid) continue;
    const Rect& r = d.rect;
    if (!(overlaps_x(e, p, r) && e.pos.y < r.top && e.pos.y + h > r.bottom)) continue;
    const Fixed up = r.top - e.pos.y;
    const Fixed left = e.pos.x + hw - r.left;
    const Fixed right = r.right - (e.pos.x - hw);
    const Fixed down = e.pos.y + h - r.bottom;
    if (up <= left && up <= right && up <= down) {
      e.pos.y = r.top;
      if (e.mode == Mode::Airborne) {
        land(e);
        e.support = static_cast<std::int16_t>(i);
        events.push_back({EventType::Landed, id, i});
      }
    } else if (left <= right && left <= down) {
      e.pos.x = r.left - hw;
      e.vel.x = Fixed::zero();
    } else if (right <= down) {
      e.pos.x = r.right + hw;
      e.vel.x = Fixed::zero();
    } else {
      e.pos.y = r.bottom - h;
      if (e.vel.y > Fixed::zero()) e.vel.y = Fixed::zero();
    }
  }

  // A grounded entity needs its support under it; otherwise it falls.
  if (grounded_mode(e.mode)) {
    bool ok = false;
    if (e.support >= 0 && e.support < n) {
      const Rect& r = stage.platforms[e.support].rect;
      ok = overlaps_x(e, p, r) && e.pos.y == r.top;
    }
    if (!ok) {
      start_fall(e);
      e.support = -1;
      events.push_back({EventType::Fell, id, -1});
    }
  }
}

}  // namespace

EntityState spawn_on(const Config& cfg, int platform, Fixed x) {
  EntityState e;
  e.pos = {x, cfg.stage.platforms[platform].rect.top};
  e.support = static_cast<std::int16_t>(platform);
  return e;
}

void step(const Config& cfg, World& w, std::span<const Intent> intents, Events& events) {
  events.clear();
  const int n = static_cast<int>(w.entities.size());
  for (int id = 0; id < n; ++id) {
    EntityState& e = w.entities[id];
    if (e.eliminated != 0) continue;
    const MovementParams& p = cfg.params[id];
    const Intent& in = intents[id];

    // A fresh down press on a pass-through platform drops through it.
    if (in.fast_fall && e.prev_down == 0 && e.mode <= Mode::Turnaround && e.support >= 0 &&
        cfg.stage.platforms[e.support].kind == PlatformKind::PassThrough) {
      start_fall(e);
      e.support = -1;
      e.drop_timer = static_cast<std::int16_t>(p.drop_through_frames);
      events.push_back({EventType::Fell, id, -1});
    }

    const Vec2 prev = e.pos;
    advance(e, in, p);
    if (e.mode == Mode::Airborne) e.support = -1;  // jumped off
    resolve_platforms(cfg.stage, e, p, prev, id, events);
    if (e.drop_timer > 0) --e.drop_timer;

    const Rect& b = cfg.stage.blast;
    if (e.pos.x < b.left || e.pos.x > b.right || e.pos.y < b.bottom || e.pos.y > b.top) {
      e.eliminated = 1;
      events.push_back({EventType::Eliminated, id, -1});
    }
  }
  ++w.frame;
}

}  // namespace fp
