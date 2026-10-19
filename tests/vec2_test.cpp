// Vec2: component arithmetic, and Wide-valued products at large coordinates.
#include "check.hpp"
#include "fp/vec2.hpp"

using fp::Fixed;
using fp::Vec2;
using fp::Wide;
using I128 = fp::detail::i128;

namespace {

Vec2 V(double x, double y) { return Vec2::from_double(x, y); }
Vec2 R(std::int32_t x, std::int32_t y) { return {Fixed::from_raw(x), Fixed::from_raw(y)}; }

long long sat64(I128 v) {
  return static_cast<long long>(v > INT64_MAX ? I128(INT64_MAX) : (v < INT64_MIN ? I128(INT64_MIN) : v));
}

void test_arithmetic() {
  static_assert(Vec2{}.x.raw() == 0 && sizeof(Vec2) == 8);
  CHECK(V(1, 2) + V(3, -5) == V(4, -3));
  CHECK(V(1, 2) - V(3, -5) == V(-2, 7));
  CHECK(-V(1, -2) == V(-1, 2));
  CHECK(V(1.5, -2) * Fixed::from_int(2) == V(3, -4));
  CHECK(Fixed::from_int(2) * V(1.5, -2) == V(3, -4));
  CHECK(V(3, -4) / Fixed::from_int(2) == V(1.5, -2));
  Vec2 v = V(1, 1);
  v += V(2, 3);
  v -= V(1, 1);
  v *= Fixed::from_int(2);
  v /= Fixed::from_int(4);
  CHECK(v == V(1, 1.5));
  // Per-component saturation.
  Vec2 hi = R(INT32_MAX, INT32_MIN);
  CHECK(hi + R(1, -1) == hi);
  CHECK(-R(INT32_MIN, INT32_MAX) == R(INT32_MAX, -INT32_MAX));
  CHECK(R(INT32_MAX, INT32_MIN) * Fixed::from_int(2) == hi);
  CHECK(V(1, -1) / Fixed::zero() == R(INT32_MAX, INT32_MIN));
}

void test_minmax() {
  CHECK(fp::abs(V(-1, 2)) == V(1, 2));
  CHECK(fp::min(V(1, 5), V(2, 4)) == V(1, 4));
  CHECK(fp::max(V(1, 5), V(2, 4)) == V(2, 5));
  CHECK(fp::clamp(V(-5, 5), V(-1, -1), V(1, 1)) == V(-1, 1));
}

void test_products() {
  CHECK(fp::dot(V(1, 2), V(3, 4)).to_double() == 11.0);
  CHECK(fp::cross(V(1, 2), V(3, 4)).to_double() == -2.0);
  CHECK(fp::cross(V(3, 4), V(1, 2)).to_double() == 2.0);
  CHECK(fp::length_squared(V(3, 4)).to_double() == 25.0);
  CHECK(fp::distance_squared(V(1, 1), V(4, 5)).to_double() == 25.0);
  CHECK(fp::dot(V(1, 0), V(0, 1)) == Wide::zero());
  // Exact at the finest resolution: (2^-16)^2 = 2^-32 is one Wide ulp.
  CHECK_EQ(fp::length_squared(R(1, 0)).raw(), 1);

  // Two positions 200 units apart in x and y: 2 * 200^2 = 80,000 > 32,768.
  Vec2 a = V(-100, -100), b = V(100, 100);
  Wide d2 = fp::distance_squared(a, b);
  CHECK(d2.to_double() == 80000.0);
  CHECK(d2 > Wide::from_fixed(Fixed::max()));  // beyond any Fixed value
  CHECK_EQ(d2.narrow().raw(), INT32_MAX);      // narrowing would have saturated
  // Collision-style comparison against a squared radius stays correct.
  Wide r2 = fp::wide_mul(Fixed::from_int(150), Fixed::from_int(150));
  CHECK(d2 > r2);
  CHECK(fp::distance_squared(a, V(-100, 49)) < r2);
  CHECK(fp::distance_squared(a, V(-100, 50)) == r2);  // exactly on the boundary
}

void test_large_coordinates() {
  const std::int32_t c[] = {INT32_MIN, INT32_MIN + 1, -65536 * 32767, -65536 * 200, -65537, -1, 0, 1,
                            65536 * 181, 65536 * 200, 65536 * 32767, INT32_MAX - 1, INT32_MAX};
  for (auto ax : c)
    for (auto ay : c)
      for (auto bx : c)
        for (auto by : c) {
          Vec2 a = R(ax, ay), b = R(bx, by);
          CHECK_EQ(fp::dot(a, b).raw(), sat64(I128(ax) * bx + I128(ay) * by));
          CHECK_EQ(fp::cross(a, b).raw(), sat64(I128(ax) * by - I128(ay) * bx));
          CHECK_EQ(fp::length_squared(a).raw(), sat64(I128(ax) * ax + I128(ay) * ay));
        }
  // Explicit saturating corners: x^2 + y^2 and x*y - y*x at +-2^31.
  Vec2 m = R(INT32_MIN, INT32_MIN);
  CHECK(fp::length_squared(m) == Wide::max());                         // 2^63 saturates
  // cross never reaches +-2^63 (its largest magnitude is 2^63 - 2^31); dot can only
  // saturate upward, and its most negative value fits.
  CHECK_EQ(fp::cross(R(INT32_MIN, INT32_MIN), R(INT32_MIN, INT32_MAX)).raw(),
           static_cast<long long>(I128(INT32_MIN) * INT32_MAX - I128(INT32_MIN) * INT32_MIN));
  CHECK_EQ(fp::dot(R(INT32_MIN, INT32_MIN), R(INT32_MAX, INT32_MAX)).raw(),
           static_cast<long long>(I128(INT32_MIN) * INT32_MAX * 2));
  // distance_squared saturates the per-component difference first: documented.
  CHECK(fp::distance_squared(R(INT32_MAX, 0), R(INT32_MIN, 0)) ==
        fp::length_squared(R(INT32_MAX, 0)));
}

}  // namespace

int main() {
  test_arithmetic();
  test_minmax();
  test_products();
  test_large_coordinates();
  return fp_test::finish();
}
