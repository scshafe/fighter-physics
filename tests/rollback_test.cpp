// Snapshot, restore and re-simulation: what netcode does. A world restored from a
// snapshot, fed the same inputs, must reproduce the original run bit for bit,
// whatever the worker count; malformed snapshots are refused.
#include <cstdio>
#include <vector>

#include "check.hpp"
#include "scenario.hpp"

using namespace fp;
using scenario::Scenario;

namespace {

std::uint64_t bytes_hash(const std::vector<std::uint8_t>& b) {
  std::uint64_t h = 0xcbf29ce484222325ULL;
  for (std::uint8_t x : b) {
    h ^= x;
    h *= 0x100000001b3ULL;
  }
  return h;
}

// Run a scenario for `frames`, returning the hash after each frame and recording the inputs.
struct Recording {
  std::vector<std::vector<Intent>> inputs;
  std::vector<std::uint64_t> hashes;  // after frame i
};

void test_round_trip_and_bytes() {
  Scenario sc(48);
  for (int f = 0; f < 150; ++f) sc.advance(sc.next_intents(), nullptr);
  std::vector<std::uint8_t> a, b;
  snapshot(sc.w, a);
  snapshot(sc.w, b);
  CHECK(a == b);
  World other = make_world(sc.cfg);
  CHECK(restore(sc.cfg, other, a));
  CHECK(hash(other) == hash(sc.w));
  snapshot(other, b);
  CHECK(a == b);
  // Size: header + frame + 2 counts + 48 entities + 6 platforms; small enough to keep a ring of them.
  std::printf("snapshot of 48 entities, 6 platforms: %zu bytes (%.1f per entity)\n", a.size(), a.size() / 48.0);
  CHECK(a.size() < 48 * 64 + 256);
  // Pinned (Linux and macOS, gcc and clang must write the same bytes).
  CHECK(bytes_hash(a) == 0xdb678eefcf22f0c0ULL);
}

// Rewind a live world and replay: same hashes as the first time, for serial and parallel replay.
void test_replay_matches() {
  Scenario sc(48);
  Recording rec;
  for (int f = 0; f < 100; ++f) sc.advance(sc.next_intents(), nullptr);
  std::vector<std::uint8_t> snap;
  snapshot(sc.w, snap);
  for (int f = 0; f < 200; ++f) {
    rec.inputs.push_back(sc.next_intents());
    sc.advance(rec.inputs.back(), nullptr);
    rec.hashes.push_back(hash(sc.w));
  }
  for (int workers : {1, 2, 4, 8}) {
    JobSystem js(workers);
    Scenario replay(48);  // only for its config and respawn logic; its world is replaced
    CHECK(restore(replay.cfg, replay.w, snap));
    for (int f = 0; f < 200; ++f) {
      replay.advance(rec.inputs[f], workers == 1 ? nullptr : &js);
      if (hash(replay.w) != rec.hashes[f]) {
        std::fprintf(stderr, "workers=%d: replay differs at frame %d after the snapshot\n", workers, f + 1);
        ++fp_test::failures();
        break;
      }
    }
  }
  // In place: rewind the very world that ran ahead.
  CHECK(restore(sc.cfg, sc.w, snap));
  for (int f = 0; f < 200; ++f) {
    sc.advance(rec.inputs[f], nullptr);
    CHECK(hash(sc.w) == rec.hashes[f]);
  }
}

// What netcode does: keep a ring of the last 8 snapshots; every 20 frames "a late input arrives",
// roll back 8 frames and re-simulate them. Each re-simulated frame must match the first time.
void test_rolling_back_repeatedly() {
  Scenario sc(16);
  const int depth = 8;
  std::vector<std::vector<std::uint8_t>> ring(depth);
  std::vector<std::vector<Intent>> inputs(400);
  std::vector<std::uint64_t> hashes(400);
  int rollbacks = 0;
  for (int f = 0; f < 400; ++f) {
    snapshot(sc.w, ring[f % depth]);  // the state before frame f
    inputs[f] = sc.next_intents();
    sc.advance(inputs[f], nullptr);
    hashes[f] = hash(sc.w);
    if (f >= depth && f % 20 == 0) {
      const int from = f - depth + 1;  // the oldest frame still in the ring
      CHECK(restore(sc.cfg, sc.w, ring[from % depth]));
      for (int g = from; g <= f; ++g) {
        sc.advance(inputs[g], nullptr);
        if (hash(sc.w) != hashes[g]) {
          std::fprintf(stderr, "rollback at frame %d: re-simulated frame %d differs\n", f, g);
          ++fp_test::failures();
          return;
        }
      }
      ++rollbacks;
    }
  }
  CHECK(rollbacks >= 15);
}

// Every PlatformPhase survives a snapshot and restore; the first value past the last one is
// refused. Add the next enumerator to `kPhases` (and keep Doomed or its successor last).
void test_every_platform_phase() {
  const PlatformPhase kPhases[] = {PlatformPhase::Solid, PlatformPhase::Shaking, PlatformPhase::Gone,
                                   PlatformPhase::Destroyed, PlatformPhase::Doomed};
  Scenario sc(4);
  const std::uint8_t past = static_cast<std::uint8_t>(PlatformPhase::Doomed) + 1;
  for (PlatformPhase ph : kPhases) {
    for (PlatformState& p : sc.w.platforms) p.phase = ph;
    std::vector<std::uint8_t> bytes;
    snapshot(sc.w, bytes);
    World back = make_world(sc.cfg);
    back.entities = sc.w.entities;
    CHECK(restore(sc.cfg, back, bytes));
    CHECK(hash(back) == hash(sc.w));
  }
  for (PlatformState& p : sc.w.platforms) p.phase = static_cast<PlatformPhase>(past);
  std::vector<std::uint8_t> bytes;
  snapshot(sc.w, bytes);
  World back = make_world(sc.cfg);
  back.entities = sc.w.entities;
  CHECK(!restore(sc.cfg, back, bytes));

  // And the real thing: a stress platform stood on until it is Doomed.
  Scenario st(1);
  st.w.entities[0] = spawn_on(st.cfg, 5, scenario::F(117));
  bool doomed = false;
  for (int f = 0; f < 600 && !doomed; ++f) {
    st.advance(std::vector<Intent>(1), nullptr);
    doomed = st.w.platforms[5].phase == PlatformPhase::Doomed;
  }
  CHECK(doomed);
  snapshot(st.w, bytes);
  World r = make_world(st.cfg);
  r.entities = st.w.entities;
  CHECK(restore(st.cfg, r, bytes) && hash(r) == hash(st.w));
}

void test_malformed() {
  Scenario sc(6);
  for (int f = 0; f < 60; ++f) sc.advance(sc.next_intents(), nullptr);
  std::vector<std::uint8_t> good;
  snapshot(sc.w, good);
  const std::uint64_t before = hash(sc.w);

  // Truncated at every length, one trailing byte, bad header: refused, world untouched.
  for (std::size_t len = 0; len < good.size(); ++len) {
    CHECK(!restore(sc.cfg, sc.w, std::span<const std::uint8_t>(good.data(), len)));
  }
  std::vector<std::uint8_t> longer = good;
  longer.push_back(0);
  CHECK(!restore(sc.cfg, sc.w, longer));
  std::vector<std::uint8_t> bad = good;
  bad[0] ^= 1;
  CHECK(!restore(sc.cfg, sc.w, bad));
  CHECK(hash(sc.w) == before);

  // Another config (different entity count) is refused.
  Scenario other(7);
  World w = make_world(other.cfg);
  CHECK(!restore(other.cfg, w, good));

  // Changing any single byte either is refused or gives a world that snapshots to exactly those bytes
  // (parsing is canonical), and never crashes or leaves a half-written world.
  int refused = 0, accepted = 0;
  for (std::size_t i = 4; i < good.size(); ++i) {
    for (std::uint8_t x : {std::uint8_t{0x01}, std::uint8_t{0x80}, std::uint8_t{0xff}}) {
      std::vector<std::uint8_t> m = good;
      m[i] ^= x;
      World t = sc.w;
      if (restore(sc.cfg, t, m)) {
        ++accepted;
        std::vector<std::uint8_t> back;
        snapshot(t, back);
        CHECK(back == m);
        // Whatever state it holds, it can be stepped.
        Events ev;
        std::vector<Intent> in(t.entities.size());
        step(sc.cfg, t, in, ev);
      } else {
        ++refused;
        CHECK(hash(t) == before);
      }
    }
  }
  CHECK(refused > 0 && accepted > 0);
  std::printf("single-byte mutations: %d refused, %d accepted as other valid states\n", refused, accepted);
}

}  // namespace

int main() {
  test_round_trip_and_bytes();
  test_replay_matches();
  test_rolling_back_repeatedly();
  test_every_platform_phase();
  test_malformed();
  return fp_test::finish();
}
