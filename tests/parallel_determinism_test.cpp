// The parallel step is bit-identical to the serial one for every worker count:
// a busy scenario (stacking entities, crumbling and moving platforms, a wall,
// eliminations and respawns) is run serially and with several worker counts, and
// the state hash and event stream must match on every frame. The final hash is
// pinned so that CI also compares Linux and macOS (and gcc and clang).
#include <cstdio>
#include <map>
#include <vector>

#include "check.hpp"
#include "scenario.hpp"

using namespace fp;

namespace {

constexpr int kEntities = 48;
constexpr int kFrames = 1500;

using scenario::F;

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
  scenario::Scenario sc(kEntities);
  std::vector<std::pair<std::uint64_t, std::uint64_t>> trace;
  for (int f = 0; f < kFrames; ++f) {
    // The hash is taken before the game respawns anyone, so it sees the step's own result.
    sc.ev.clear();
    sc.place_scheduled();
    step(sc.cfg, sc.w, sc.next_intents(), sc.ev, jobs);
    std::uint64_t eh = 0;
    for (const Event& e : sc.ev) {
      eh = mix(eh, (static_cast<std::uint64_t>(e.type) << 40) ^ (static_cast<std::uint64_t>(e.id) << 20) ^
                       static_cast<std::uint64_t>(e.other + 1));
      if (totals) ++totals->events[e.type];
    }
    trace.emplace_back(hash(sc.w), eh);
    for (int i = 0; i < kEntities; ++i) {  // the "game": eliminated entities come back at the top
      if (sc.w.entities[i].eliminated != 0) {
        sc.w.entities[i] = scenario::spawn(i);
        sc.w.entities[i].pos.y = F(60);
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
                      EventType::PlatformRespawned, EventType::PlatformDoomed, EventType::PlaceableRemoved, EventType::Sprung}) {
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
  CHECK(serial.back().first == 0x5707871cc3ad8a4fULL);
  std::printf("final hash %016llx; events: landed %d, fell %d, eliminated %d, on-entity %d, bumped %d, shaking %d, "
              "crumbled %d, respawned %d; entity respawns %d\n",
              static_cast<unsigned long long>(serial.back().first), totals.events[EventType::Landed],
              totals.events[EventType::Fell], totals.events[EventType::Eliminated],
              totals.events[EventType::LandedOnEntity], totals.events[EventType::BumpedEntity],
              totals.events[EventType::PlatformShaking], totals.events[EventType::PlatformCrumbled],
              totals.events[EventType::PlatformRespawned], totals.respawns);
  return fp_test::finish();
}
