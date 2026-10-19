#pragma once
// The engine's own job system: a fixed number of workers (the calling thread is
// worker 0) and one primitive, parallel_for, with a static partition. Index i
// always runs on the same worker for a given count and worker count, and a job
// may only write data owned by its index, so results never depend on timing or
// on how many workers there are.

#include <memory>
#include <type_traits>

namespace fp {

class JobSystem {
 public:
  // `workers` is the total number of threads that run jobs, caller included
  // (>= 1; 1 runs everything on the caller, with no threads).
  explicit JobSystem(int workers);
  ~JobSystem();
  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;

  int workers() const noexcept;

  // Calls fn(i) once for every i in [0, count), worker w taking the contiguous
  // range [count * w / W, count * (w + 1) / W), and returns when all are done.
  // `fn` must not throw and must not call parallel_for. One caller at a time.
  template <class F>
  void parallel_for(int count, F&& fn) {
    using Fn = std::remove_reference_t<F>;
    run(count, [](void* ctx, int i) { (*static_cast<Fn*>(ctx))(i); }, const_cast<std::remove_const_t<Fn>*>(&fn));
  }

 private:
  void run(int count, void (*fn)(void*, int), void* ctx);
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace fp
