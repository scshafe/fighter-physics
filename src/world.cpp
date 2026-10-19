#include "fp/world.hpp"

namespace fp {
namespace {

// What contact resolution sees of a platform this frame: where it is and
// whether it is in play. Computed once per step, read-only afterwards.
struct View {
  Rect rect;
  PlatformKind kind;
  bool active;
};

bool overlaps_x(const EntityState& e, const MovementParams& p, const Rect& r) noexcept {
  return e.pos.x + p.body_half_width > r.left && e.pos.x - p.body_half_width < r.right;
}

bool grounded_mode(Mode m) noexcept { return m <= Mode::Turnaround || m == Mode::JumpSquat; }

bool in_play(PlatformPhase ph) noexcept { return ph == PlatformPhase::Solid || ph == PlatformPhase::Shaking; }

// Resolve one entity's contacts with the platforms after `advance` moved it
// from `prev`. Platforms are static during contact resolution (they move at
// the end of the frame), so every entity's result depends only on its own
// motion and the stage.
void resolve_platforms(const std::vector<View>& views, EntityState& e, const MovementParams& p, Vec2 prev, int id,
                       Events& events) {
  const int n = static_cast<int>(views.size());
  const Fixed hw = p.body_half_width, h = p.body_height;

  // Horizontal: solid platforms are walls (and the sides of floors).
  for (int i = 0; i < n; ++i) {
    const View& v = views[i];
    if (!v.active || v.kind != PlatformKind::Solid) continue;
    const Rect& r = v.rect;
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
        const View& v = views[i];
        if (!v.active) continue;
        if (v.kind == PlatformKind::PassThrough && e.drop_timer > 0) continue;
        const Rect& r = v.rect;
        if (prev.y >= r.top && e.pos.y <= r.top && overlaps_x(e, p, r) && (landed < 0 || r.top > best)) {
          landed = i;
          best = r.top;
        }
      }
    } else {
      int bump = -1;
      Fixed best{};
      for (int i = 0; i < n; ++i) {
        const View& v = views[i];
        if (!v.active || v.kind != PlatformKind::Solid) continue;
        const Rect& r = v.rect;
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
    e.pos.y = views[landed].rect.top;
    land(e);
    e.support = static_cast<std::int16_t>(landed);
    events.push_back({EventType::Landed, id, landed});
  }

  // Still inside a solid (it moved or respawned onto the entity, or the entity
  // was placed there): push out along the shallowest way.
  for (int i = 0; i < n; ++i) {
    const View& v = views[i];
    if (!v.active || v.kind != PlatformKind::Solid) continue;
    const Rect& r = v.rect;
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
    if (e.support >= 0 && e.support < n && views[e.support].active) {
      const Rect& r = views[e.support].rect;
      ok = overlaps_x(e, p, r) && e.pos.y == r.top;
    }
    if (!ok) {
      start_fall(e);
      e.support = -1;
      events.push_back({EventType::Fell, id, -1});
    }
  }
}

std::vector<View> make_views(const Config& cfg, const World& w) {
  const int n = static_cast<int>(cfg.stage.platforms.size());
  std::vector<View> views(n);
  for (int i = 0; i < n; ++i) {
    const PlatformDef& d = cfg.stage.platforms[i];
    const PlatformState& s = w.platforms[i];
    Rect r = d.rect;
    r.left += s.offset.x;
    r.right += s.offset.x;
    r.bottom += s.offset.y;
    r.top += s.offset.y;
    views[i] = {r, d.kind, in_play(s.phase)};
  }
  return views;
}

// Entities standing on `platform` fall (it vanished).
void release_riders(World& w, int platform, Events& events) {
  for (int id = 0; id < static_cast<int>(w.entities.size()); ++id) {
    EntityState& e = w.entities[id];
    if (e.support == platform) {
      start_fall(e);
      e.support = -1;
      events.push_back({EventType::Fell, id, -1});
    }
  }
}

Vec2 motion_offset(const PlatformDef& d, std::int32_t clock) {
  const std::int32_t half = d.move_period / 2;
  const std::int32_t t = clock <= half ? clock : d.move_period - clock;
  const Fixed f = Fixed::from_int(t) / Fixed::from_int(half);
  return d.travel * f;
}

// Phase 6: crumble timers, respawn, motion; riders move with their platform.
void update_platforms(const Config& cfg, World& w, Events& events) {
  const int n = static_cast<int>(cfg.stage.platforms.size());
  for (int i = 0; i < n; ++i) {
    const PlatformDef& d = cfg.stage.platforms[i];
    PlatformState& s = w.platforms[i];

    bool stood_on = false;
    for (const EntityState& e : w.entities) stood_on = stood_on || e.support == i;

    switch (s.phase) {
      case PlatformPhase::Solid:
        if (d.shake_frames > 0 && stood_on) {
          s.phase = PlatformPhase::Shaking;
          s.timer = d.shake_frames;
          events.push_back({EventType::PlatformShaking, i, -1});
        }
        break;
      case PlatformPhase::Shaking:
        if (--s.timer <= 0) {
          s.phase = PlatformPhase::Gone;
          s.timer = d.respawn_frames;
          events.push_back({EventType::PlatformCrumbled, i, -1});
          release_riders(w, i, events);
        }
        break;
      case PlatformPhase::Gone:
        if (d.respawn_frames > 0 && --s.timer <= 0) {
          s.phase = PlatformPhase::Solid;
          events.push_back({EventType::PlatformRespawned, i, -1});
        }
        break;
      case PlatformPhase::Destroyed:
        break;
    }

    if (d.move_period > 0) {
      s.clock = (s.clock + 1) % d.move_period;
      const Vec2 next = motion_offset(d, s.clock);
      const Vec2 delta = next - s.offset;
      s.offset = next;
      if (in_play(s.phase)) {
        for (EntityState& e : w.entities)
          if (e.support == i) e.pos += delta;
      }
    }
  }
}

}  // namespace

World make_world(const Config& cfg) {
  World w;
  w.platforms.assign(cfg.stage.platforms.size(), PlatformState{});
  return w;
}

EntityState spawn_on(const Config& cfg, int platform, Fixed x) {
  EntityState e;
  e.pos = {x, cfg.stage.platforms[platform].rect.top};
  e.support = static_cast<std::int16_t>(platform);
  return e;
}

void destroy_platform(const Config&, World& w, int platform, Events& events) {
  PlatformState& s = w.platforms[platform];
  if (s.phase == PlatformPhase::Destroyed) return;
  const bool was_in_play = in_play(s.phase);
  s.phase = PlatformPhase::Destroyed;
  s.timer = 0;
  events.push_back({EventType::PlatformDestroyed, platform, -1});
  if (was_in_play) release_riders(w, platform, events);
}

void step(const Config& cfg, World& w, std::span<const Intent> intents, Events& events) {
  const std::vector<View> views = make_views(cfg, w);
  const int n = static_cast<int>(w.entities.size());
  for (int id = 0; id < n; ++id) {
    EntityState& e = w.entities[id];
    if (e.eliminated != 0) continue;
    const MovementParams& p = cfg.params[id];
    const Intent& in = intents[id];

    // A fresh down press on a pass-through platform drops through it.
    if (in.fast_fall && e.prev_down == 0 && e.mode <= Mode::Turnaround && e.support >= 0 &&
        views[e.support].kind == PlatformKind::PassThrough) {
      start_fall(e);
      e.support = -1;
      e.drop_timer = static_cast<std::int16_t>(p.drop_through_frames);
      events.push_back({EventType::Fell, id, -1});
    }

    const Vec2 prev = e.pos;
    advance(e, in, p);
    if (e.mode == Mode::Airborne) e.support = -1;  // jumped off
    resolve_platforms(views, e, p, prev, id, events);
    if (e.drop_timer > 0) --e.drop_timer;

    const Rect& b = cfg.stage.blast;
    if (e.pos.x < b.left || e.pos.x > b.right || e.pos.y < b.bottom || e.pos.y > b.top) {
      e.eliminated = 1;
      e.support = -1;
      events.push_back({EventType::Eliminated, id, -1});
    }
  }
  update_platforms(cfg, w, events);
  ++w.frame;
}

}  // namespace fp
