// The job system: every index runs exactly once, on the worker the static
// partition assigns, for any count and worker count.
#include <atomic>
#include <thread>
#include <vector>

#include "check.hpp"
#include "fp/jobs.hpp"

using namespace fp;

namespace {

void test_every_index_once() {
  for (int workers : {1, 2, 3, 4, 7, 16}) {
    JobSystem js(workers);
    CHECK_EQ(js.workers(), workers);
    for (int count : {0, 1, 2, 3, 5, 16, 17, 100, 1000}) {
      std::vector<int> hits(count, 0);
      js.parallel_for(count, [&](int i) { ++hits[i]; });  // each index touches only its own slot
      for (int i = 0; i < count; ++i) CHECK_EQ(hits[i], 1);
    }
  }
}

// Index i runs on the worker whose range contains it: with 4 workers and 8
// indices, indices 2k and 2k+1 share a thread, and the four threads differ.
void test_static_partition() {
  JobSystem js(4);
  std::vector<std::thread::id> who(8);
  js.parallel_for(8, [&](int i) { who[i] = std::this_thread::get_id(); });
  for (int k = 0; k < 4; ++k) CHECK(who[2 * k] == who[2 * k + 1]);
  for (int a = 0; a < 4; ++a)
    for (int b = a + 1; b < 4; ++b) CHECK(who[2 * a] != who[2 * b]);
  CHECK(who[0] == std::this_thread::get_id());  // the caller is worker 0
}

void test_single_worker_is_inline() {
  JobSystem js(1);
  std::vector<std::thread::id> who(5);
  js.parallel_for(5, [&](int i) { who[i] = std::this_thread::get_id(); });
  for (auto id : who) CHECK(id == std::this_thread::get_id());
  JobSystem zero(0);  // clamped to 1
  CHECK_EQ(zero.workers(), 1);
}

// Results written by index equal the serial results, over many back-to-back jobs.
void test_repeated_and_matches_serial() {
  JobSystem js(5);
  const int n = 257;
  std::vector<long long> par(n), ser(n);
  for (int round = 0; round < 2000; ++round) {
    js.parallel_for(n, [&](int i) { par[i] = static_cast<long long>(i) * i + round; });
    for (int i = 0; i < n; ++i) ser[i] = static_cast<long long>(i) * i + round;
    if (par != ser) {
      CHECK(false);
      break;
    }
  }
}

// Stress for the thread sanitizer: many small jobs, destroyed while idle.
void test_create_destroy() {
  for (int i = 0; i < 50; ++i) {
    JobSystem js(4);
    std::atomic<int> sum{0};
    js.parallel_for(10, [&](int k) { sum += k; });
    CHECK_EQ(sum.load(), 45);
  }
}

}  // namespace

int main() {
  test_every_index_once();
  test_static_partition();
  test_single_worker_is_inline();
  test_repeated_and_matches_serial();
  test_create_destroy();
  return fp_test::finish();
}
