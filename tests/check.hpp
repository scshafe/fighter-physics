#pragma once
// A minimal check helper for plain CTest executables (no test framework).
// Failures are counted and printed; `return fp_test::finish();` from main.

#include <cstdint>
#include <cstdio>

namespace fp_test {

inline int& failures() {
  static int n = 0;
  return n;
}

inline int finish() {
  if (failures() != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", failures());
    return 1;
  }
  return 0;
}

// Fixed seed, platform-independent: splitmix64.
class Rng {
 public:
  explicit Rng(std::uint64_t seed) : s_(seed) {}
  std::uint64_t next() {
    std::uint64_t z = (s_ += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
  }

 private:
  std::uint64_t s_;
};

}  // namespace fp_test

#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++fp_test::failures();                                                     \
    }                                                                            \
  } while (0)

// Compares two integers (any integral type) and prints both on failure.
#define CHECK_EQ(a, b)                                                           \
  do {                                                                           \
    long long va_ = static_cast<long long>(a), vb_ = static_cast<long long>(b);  \
    if (va_ != vb_) {                                                            \
      std::fprintf(stderr, "%s:%d: CHECK_EQ failed: %s = %lld, %s = %lld\n",     \
                   __FILE__, __LINE__, #a, va_, #b, vb_);                        \
      ++fp_test::failures();                                                     \
    }                                                                            \
  } while (0)
