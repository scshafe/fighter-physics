#include "fp/jobs.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace fp {

struct JobSystem::Impl {
  int workers = 1;
  std::vector<std::thread> threads;
  std::mutex m;
  std::condition_variable wake, done;
  std::uint64_t generation = 0;
  int pending = 0;
  bool stop = false;
  int count = 0;
  void (*fn)(void*, int) = nullptr;
  void* ctx = nullptr;

  static void run_range(int count, int workers, int w, void (*fn)(void*, int), void* ctx) {
    const int begin = static_cast<int>(static_cast<std::int64_t>(count) * w / workers);
    const int end = static_cast<int>(static_cast<std::int64_t>(count) * (w + 1) / workers);
    for (int i = begin; i < end; ++i) fn(ctx, i);
  }

  void worker_main(int w) {
    std::uint64_t seen = 0;
    for (;;) {
      std::unique_lock lock(m);
      wake.wait(lock, [&] { return stop || generation != seen; });
      if (stop) return;
      seen = generation;
      const int c = count;
      auto f = fn;
      void* x = ctx;
      lock.unlock();
      run_range(c, workers, w, f, x);
      lock.lock();
      if (--pending == 0) done.notify_one();
    }
  }
};

JobSystem::JobSystem(int workers) : impl_(std::make_unique<Impl>()) {
  impl_->workers = workers < 1 ? 1 : workers;
  for (int w = 1; w < impl_->workers; ++w) impl_->threads.emplace_back([this, w] { impl_->worker_main(w); });
}

JobSystem::~JobSystem() {
  {
    std::lock_guard lock(impl_->m);
    impl_->stop = true;
  }
  impl_->wake.notify_all();
  for (std::thread& t : impl_->threads) t.join();
}

int JobSystem::workers() const noexcept { return impl_->workers; }

void JobSystem::run(int count, void (*fn)(void*, int), void* ctx) {
  Impl& s = *impl_;
  if (count <= 0) return;
  if (s.workers == 1) {
    for (int i = 0; i < count; ++i) fn(ctx, i);
    return;
  }
  {
    std::lock_guard lock(s.m);
    s.count = count;
    s.fn = fn;
    s.ctx = ctx;
    s.pending = s.workers - 1;
    ++s.generation;
  }
  s.wake.notify_all();
  Impl::run_range(count, s.workers, 0, fn, ctx);
  std::unique_lock lock(s.m);
  s.done.wait(lock, [&] { return s.pending == 0; });
}

}  // namespace fp
