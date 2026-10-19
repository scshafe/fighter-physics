// The parallel step is bit-identical to the serial one for every worker count:
// a busy scenario (stacking entities, crumbling and moving platforms, a wall,
// eliminations and respawns) is run serially and with several worker counts, and
// the state hash and event stream must match on every frame. The final hash is
// pinned so that CI also compares Linux and macOS (and gcc and clang).
#include <cstdio>
#include <map>
#include <vector>

#include "check.hpp"
#include "fp/jobs.hpp"
#include "fp/state.hpp"
#include "fp/world.hpp"

using namespace fp;

namespace {

constexpr int kEntities = 48;
constexpr int kFrames = 1500;

Fixed F(double v) { return Fixed::from_double(v); }

Config make_config() {
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
  PlatformDef far_floor = solid(94, 140, -10, 0);
  c.stage.platforms = {floor, shelf, crumble, mover, wall, far_floor};
  c.stage.blast = {F(-140), F(150), F(-40), F(120)};
  c.params.assign(kEntities, MovementParams{});
  for (int i = 0; i < kEntities; ++i) {  // a few different archetypes
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

EntityState spawn(const Config& cfg, int id) {
  // Spread over the platforms, several at the same x so they stack on landing.
  EntityState e;
  e.mode = Mode::Airborne;
  e.pos = Vec2::from_double(-50.0 + 10.0 * (id % 8) + (id % 5 == 0 ? 0.5 : 0.0), 30.0 + 7.0 * (id / 8));
  (void)cfg;
  return e;
}

struct Totals {
  std::map<EventType, int> events;
  int respawns = 0;
};

std::uint64_t mix(std::uint64_t h, std::uint64_t v) {
  h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
  return h * 0xff51afd7ed558ccdULL;
}

// Returns the per-frame (state hash, event hash) trace.
std::vector<std::pair<std::uint64_t, std::uint64_t>> run(JobSystem* jobs, Totals* totals) {
  const Config cfg = make_config();
  World w = make_world(cfg);
  for (int i = 0; i < kEntities; ++i) w.entities.push_back(spawn(cfg, i));
  fp_test::Rng rng(0xf16417e5ULL);
  std::vector<Intent> in(kEntities);
  std::vector<std::pair<std::uint64_t, std::uint64_t>> trace;
  Events ev;
  for (int f = 0; f < kFrames; ++f) {
    for (int i = 0; i < kEntities; ++i) {
      const std::uint64_t r = rng.next();
      Intent& x = in[i];
      if ((r & 7) == 0) x.move_x = F(static_cast<int>((r >> 8) % 5) * 0.5 - 1.0);  // sticky: changes now and then
      if (((r >> 16) & 7) == 0) x.jump = !x.jump;
      x.fast_fall = ((r >> 24) & 15) == 0;
    }
    ev.clear();
    step(cfg, w, in, ev, jobs);
    std::uint64_t eh = 0;
    for (const Event& e : ev) {
      eh = mix(eh, (static_cast<std::uint64_t>(e.type) << 40) ^ (static_cast<std::uint64_t>(e.id) << 20) ^
                       static_cast<std::uint64_t>(e.other + 1));
      if (totals) ++totals->events[e.type];
    }
    trace.emplace_back(hash(w), eh);
    // The "game": eliminated entities come back at the top.
    for (int i = 0; i < kEntities; ++i) {
      if (w.entities[i].eliminated != 0) {
        w.entities[i] = spawn(cfg, i);
        w.entities[i].pos.y = F(60);
        if (totals) ++totals->respawns;
      }
    }
  }
  return trace;
}

}  // namespace

int main() {
  Totals totals;
  const auto serial = run(nullptr, &totals);

  // The scenario must actually exercise the interesting paths.
  for (EventType t : {EventType::Landed, EventType::Fell, EventType::Eliminated, EventType::LandedOnEntity,
                      EventType::BumpedEntity, EventType::PlatformShaking, EventType::PlatformCrumbled,
                      EventType::PlatformRespawned}) {
    if (totals.events[t] == 0) {
      std::fprintf(stderr, "scenario never produced event type %d\n", static_cast<int>(t));
      ++fp_test::failures();
    }
  }
  CHECK(totals.respawns > 0);

  for (int workers : {1, 2, 3, 4, 5, 8}) {
    JobSystem js(workers);
    const auto par = run(&js, nullptr);
    CHECK_EQ(par.size(), serial.size());
    for (std::size_t f = 0; f < serial.size() && f < par.size(); ++f) {
      if (par[f] != serial[f]) {
        std::fprintf(stderr, "workers=%d: first difference at frame %zu (state %s, events %s)\n", workers, f + 1,
                     par[f].first != serial[f].first ? "differ" : "same",
                     par[f].second != serial[f].second ? "differ" : "same");
        ++fp_test::failures();
        break;
      }
    }
  }

  // Pinned: the same on Linux and macOS, gcc and clang, debug and release. If the simulation changes on
  // purpose, update it deliberately (the line below prints the new value).
  CHECK(serial.back().first == 0x779c07337e215c5aULL);
  std::printf("final hash %016llx; events: landed %d, fell %d, eliminated %d, on-entity %d, bumped %d, shaking %d, "
              "crumbled %d, respawned %d; entity respawns %d\n",
              static_cast<unsigned long long>(serial.back().first), totals.events[EventType::Landed],
              totals.events[EventType::Fell], totals.events[EventType::Eliminated],
              totals.events[EventType::LandedOnEntity], totals.events[EventType::BumpedEntity],
              totals.events[EventType::PlatformShaking], totals.events[EventType::PlatformCrumbled],
              totals.events[EventType::PlatformRespawned], totals.respawns);
  return fp_test::finish();
}
