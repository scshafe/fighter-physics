// Rollback cost: what netcode pays to snapshot, restore, hash, step and to
// re-simulate N frames from a snapshot. Not a test: run a Release build and read
// the numbers (`--quick` runs a few iterations, for CI to check that it works).
//
//   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release
//   ./build-release/fighter_physics_rollback_bench
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../tests/scenario.hpp"

using namespace fp;
using Clock = std::chrono::steady_clock;

namespace {

template <class F>
double micros_per_call(int iterations, F&& f) {
  const auto t0 = Clock::now();
  for (int i = 0; i < iterations; ++i) f();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::micro>(t1 - t0).count() / iterations;
}

void bench(int entities, int workers, int iterations) {
  scenario::Scenario sc(entities);
  for (int f = 0; f < 300; ++f) sc.advance(sc.next_intents(), nullptr);  // get past the initial drop
  std::vector<std::vector<Intent>> inputs;
  for (int f = 0; f < 8; ++f) inputs.push_back(sc.next_intents());

  JobSystem js(workers);
  JobSystem* jobs = workers > 1 ? &js : nullptr;
  std::vector<std::uint8_t> snap;
  snapshot(sc.w, snap);
  volatile std::uint64_t sink = 0;

  const double t_snapshot = micros_per_call(iterations * 20, [&] { snapshot(sc.w, snap); });
  const double t_restore = micros_per_call(iterations * 20, [&] { restore(sc.cfg, sc.w, snap); });
  const double t_hash = micros_per_call(iterations * 20, [&] { sink = sink + hash(sc.w); });
  restore(sc.cfg, sc.w, snap);
  const double t_step = micros_per_call(iterations * 4, [&] { sc.advance(inputs[0], jobs); });
  restore(sc.cfg, sc.w, snap);
  const double t_rollback = micros_per_call(iterations, [&] {
    restore(sc.cfg, sc.w, snap);
    for (int f = 0; f < 8; ++f) sc.advance(inputs[f], jobs);
    sink = sink + hash(sc.w);
  });
  std::printf("%3d entities, %d worker%s: snapshot %7.2f us (%zu B)  restore %7.2f us  hash %6.2f us  "
              "step %8.2f us  rollback of 8 frames %9.2f us (%.1f%% of a 60 fps frame)\n",
              entities, workers, workers == 1 ? " " : "s", t_snapshot, snap.size(), t_restore, t_hash, t_step,
              t_rollback, 100.0 * t_rollback / 16667.0);
}

}  // namespace

int main(int argc, char** argv) {
  const bool quick = argc > 1 && std::strcmp(argv[1], "--quick") == 0;
  const int iterations = quick ? 2 : 200;
  for (int entities : {8, 48, 256})
    for (int workers : {1, 4}) bench(entities, workers, iterations);
  return 0;
}
