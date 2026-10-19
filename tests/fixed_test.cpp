// Fixed (Q16.16) and Wide (Q32.32): rounding, saturation, multiply/divide,
// edge conversions. Results are checked against an independent __int128
// reference at hand-picked edges (all pairs) and on a fixed-seed random sample.
#include <cmath>
#include <limits>
#include <vector>

#include "check.hpp"
#include "fp/fixed.hpp"

using fp::Fixed;
using fp::Wide;
using I128 = fp::detail::i128;

namespace {

constexpr I128 kMax = INT32_MAX;
constexpr I128 kMin = INT32_MIN;

I128 sat(I128 v) { return v > kMax ? kMax : (v < kMin ? kMin : v); }

// Round the rational n/d (d != 0) to the nearest integer, ties toward +inf,
// by exact remainder comparison.
I128 ref_round_div(I128 n, I128 d) {
  if (d < 0) {
    n = -n;
    d = -d;
  }
  I128 q = n / d, r = n % d;
  if (r < 0) {
    --q;
    r += d;
  }  // now n = q*d + r, 0 <= r < d
  return (2 * r >= d) ? q + 1 : q;
}

I128 ref_add(int a, int b) { return sat(I128(a) + b); }
I128 ref_sub(int a, int b) { return sat(I128(a) - b); }
I128 ref_mul(int a, int b) { return sat(ref_round_div(I128(a) * b, 65536)); }
I128 ref_div(int a, int b) {
  if (b == 0) return a > 0 ? kMax : (a < 0 ? kMin : 0);
  return sat(ref_round_div(I128(a) * 65536, b));
}

std::vector<std::int32_t> edge_values() {
  return {INT32_MIN, INT32_MIN + 1, -65536 * 200, -65537, -65536, -65535, -32769, -32768, -3, -2, -1,
          0, 1, 2, 3, 21845, 32767, 32768, 32769, 65535, 65536, 65537, 65536 * 181,
          65536 * 200, INT32_MAX - 1, INT32_MAX};
}

void check_pair(std::int32_t a, std::int32_t b) {
  Fixed fa = Fixed::from_raw(a), fb = Fixed::from_raw(b);
  CHECK_EQ((fa + fb).raw(), ref_add(a, b));
  CHECK_EQ((fa - fb).raw(), ref_sub(a, b));
  CHECK_EQ((fa * fb).raw(), ref_mul(a, b));
  CHECK_EQ((fa / fb).raw(), ref_div(a, b));
  // wide_mul is exact and narrows to the rounded Fixed product.
  CHECK_EQ(fp::wide_mul(fa, fb).raw(), static_cast<long long>(I128(a) * b));
  CHECK_EQ(fp::wide_mul(fa, fb).narrow().raw(), ref_mul(a, b));
}

void test_constants_and_conversion() {
  static_assert(Fixed::one().raw() == 65536);
  static_assert(Fixed::from_double(0.5).raw() == 32768);
  static_assert(Fixed::from_double(-1.25).raw() == -81920);
  static_assert(sizeof(Fixed) == 4 && sizeof(Wide) == 8);
  CHECK_EQ(Fixed::from_int(3).raw(), 3 * 65536);
  CHECK_EQ(Fixed::from_int(-32768).raw(), INT32_MIN);
  CHECK_EQ(Fixed::from_int(32768).raw(), INT32_MAX);  // saturates
  CHECK_EQ(Fixed::from_int(INT32_MIN).raw(), INT32_MIN);
  CHECK_EQ(Fixed::from_int(INT32_MAX).raw(), INT32_MAX);

  // from_double: nearest, ties away from zero.
  CHECK_EQ(Fixed::from_double(0.5 / 65536).raw(), 1);
  CHECK_EQ(Fixed::from_double(-0.5 / 65536).raw(), -1);
  CHECK_EQ(Fixed::from_double(0.49 / 65536).raw(), 0);
  CHECK_EQ(Fixed::from_double(-0.49 / 65536).raw(), 0);
  CHECK_EQ(Fixed::from_double(1.5 / 65536).raw(), 2);
  CHECK_EQ(Fixed::from_double(-1.5 / 65536).raw(), -2);
  CHECK_EQ(Fixed::from_double(0.08).raw(), 5243);  // 5242.88
  // Saturation, NaN, infinity.
  CHECK_EQ(Fixed::from_double(40000.0).raw(), INT32_MAX);
  CHECK_EQ(Fixed::from_double(-40000.0).raw(), INT32_MIN);
  CHECK_EQ(Fixed::from_double(1e300).raw(), INT32_MAX);
  CHECK_EQ(Fixed::from_double(-1e300).raw(), INT32_MIN);
  CHECK_EQ(Fixed::from_double(std::numeric_limits<double>::infinity()).raw(), INT32_MAX);
  CHECK_EQ(Fixed::from_double(-std::numeric_limits<double>::infinity()).raw(), INT32_MIN);
  CHECK_EQ(Fixed::from_double(std::numeric_limits<double>::quiet_NaN()).raw(), 0);
  CHECK_EQ(Fixed::from_double(32767.999999).raw(), INT32_MAX);
  CHECK_EQ(Fixed::from_double(-32768.0).raw(), INT32_MIN);

  // to_double is exact, so a round trip is the identity for every raw value.
  for (std::int64_t r = INT32_MIN; r <= INT32_MAX; r += 1009) {
    Fixed f = Fixed::from_raw(static_cast<std::int32_t>(r));
    CHECK_EQ(Fixed::from_double(f.to_double()).raw(), f.raw());
  }
  CHECK_EQ(Fixed::from_double(Fixed::max().to_double()).raw(), INT32_MAX);
  CHECK_EQ(Fixed::from_double(Fixed::min().to_double()).raw(), INT32_MIN);
  CHECK(Fixed::from_raw(1).to_double() == 1.0 / 65536);

  CHECK_EQ(Fixed::from_double(2.75).floor_int(), 2);
  CHECK_EQ(Fixed::from_double(-2.75).floor_int(), -3);
  CHECK_EQ(Fixed::from_double(-3.0).floor_int(), -3);
  CHECK_EQ(Fixed::min().floor_int(), -32768);
}

void test_add_sub_neg() {
  CHECK_EQ((Fixed::max() + Fixed::epsilon()).raw(), INT32_MAX);
  CHECK_EQ((Fixed::min() + Fixed::from_raw(-1)).raw(), INT32_MIN);
  CHECK_EQ((Fixed::min() - Fixed::epsilon()).raw(), INT32_MIN);
  CHECK_EQ((Fixed::max() - Fixed::from_raw(-1)).raw(), INT32_MAX);
  CHECK_EQ((Fixed::max() + Fixed::min()).raw(), -1);
  CHECK_EQ((-Fixed::min()).raw(), INT32_MAX);
  CHECK_EQ((-Fixed::max()).raw(), -INT32_MAX);
  CHECK_EQ(fp::abs(Fixed::min()).raw(), INT32_MAX);
  CHECK_EQ(fp::abs(Fixed::from_raw(-5)).raw(), 5);
  Fixed x = Fixed::from_int(1);
  x += Fixed::from_int(2);
  x -= Fixed::from_int(5);
  CHECK_EQ(x.raw(), -2 * 65536);
}

void test_mul() {
  Fixed half = Fixed::from_double(0.5);
  // Ties go toward +infinity: +0.5 raw -> 1, -0.5 raw -> 0.
  CHECK_EQ((Fixed::from_raw(1) * half).raw(), 1);
  CHECK_EQ((Fixed::from_raw(-1) * half).raw(), 0);
  CHECK_EQ((Fixed::from_raw(3) * half).raw(), 2);    // 1.5 -> 2
  CHECK_EQ((Fixed::from_raw(-3) * half).raw(), -1);  // -1.5 -> -1
  CHECK_EQ((Fixed::from_raw(5) * half).raw(), 3);    // 2.5 -> 3
  CHECK_EQ((Fixed::from_raw(-5) * half).raw(), -2);  // -2.5 -> -2
  // Below and above the tie.
  CHECK_EQ((Fixed::from_raw(1) * Fixed::from_raw(32767)).raw(), 0);
  CHECK_EQ((Fixed::from_raw(1) * Fixed::from_raw(-32769)).raw(), -1);
  // Identity, zero.
  CHECK_EQ((Fixed::from_raw(12345) * Fixed::one()).raw(), 12345);
  CHECK_EQ((Fixed::from_raw(-12345) * Fixed::zero()).raw(), 0);
  // Ordinary and at the edge of range.
  CHECK_EQ((Fixed::from_int(181) * Fixed::from_int(181)).raw(), 32761 * 65536);
  CHECK_EQ((Fixed::from_int(-181) * Fixed::from_int(181)).raw(), -32761 * 65536);
  // Overflow saturates, with the right sign.
  CHECK_EQ((Fixed::from_int(200) * Fixed::from_int(200)).raw(), INT32_MAX);
  CHECK_EQ((Fixed::from_int(-200) * Fixed::from_int(-200)).raw(), INT32_MAX);
  CHECK_EQ((Fixed::from_int(-200) * Fixed::from_int(200)).raw(), INT32_MIN);
  CHECK_EQ((Fixed::min() * Fixed::min()).raw(), INT32_MAX);
  CHECK_EQ((Fixed::min() * Fixed::from_int(-1)).raw(), INT32_MAX);
  CHECK_EQ((Fixed::max() * Fixed::max()).raw(), INT32_MAX);
  CHECK_EQ((Fixed::min() * Fixed::max()).raw(), INT32_MIN);
  CHECK_EQ((Fixed::min() * Fixed::one()).raw(), INT32_MIN);
}

void test_div() {
  CHECK_EQ((Fixed::one() / Fixed::from_int(2)).raw(), 32768);
  CHECK_EQ((Fixed::one() / Fixed::from_int(3)).raw(), 21845);       // 21845.33
  CHECK_EQ((Fixed::from_int(2) / Fixed::from_int(3)).raw(), 43691);  // 43690.67
  CHECK_EQ((Fixed::from_int(-2) / Fixed::from_int(3)).raw(), -43691);
  CHECK_EQ((Fixed::from_int(2) / Fixed::from_int(-3)).raw(), -43691);
  CHECK_EQ((Fixed::from_int(-2) / Fixed::from_int(-3)).raw(), 43691);
  // Ties toward +infinity: 1 raw / 2 = 0.5 raw -> 1; -1 raw / 2 -> 0.
  CHECK_EQ((Fixed::from_raw(1) / Fixed::from_int(2)).raw(), 1);
  CHECK_EQ((Fixed::from_raw(-1) / Fixed::from_int(2)).raw(), 0);
  CHECK_EQ((Fixed::from_raw(1) / Fixed::from_int(-2)).raw(), 0);
  CHECK_EQ((Fixed::from_raw(-1) / Fixed::from_int(-2)).raw(), 1);
  CHECK_EQ((Fixed::from_raw(3) / Fixed::from_int(2)).raw(), 2);    // 1.5 -> 2
  CHECK_EQ((Fixed::from_raw(-3) / Fixed::from_int(2)).raw(), -1);  // -1.5 -> -1
  // Division by zero saturates by the numerator's sign; 0/0 = 0.
  CHECK_EQ((Fixed::one() / Fixed::zero()).raw(), INT32_MAX);
  CHECK_EQ((Fixed::from_raw(-1) / Fixed::zero()).raw(), INT32_MIN);
  CHECK_EQ((Fixed::zero() / Fixed::zero()).raw(), 0);
  CHECK_EQ((Fixed::min() / Fixed::zero()).raw(), INT32_MIN);
  // Overflow.
  CHECK_EQ((Fixed::min() / Fixed::from_int(-1)).raw(), INT32_MAX);
  CHECK_EQ((Fixed::from_int(30000) / Fixed::from_double(0.5)).raw(), INT32_MAX);
  CHECK_EQ((Fixed::from_int(-30000) / Fixed::from_double(0.5)).raw(), INT32_MIN);
  CHECK_EQ((Fixed::max() / Fixed::epsilon()).raw(), INT32_MAX);
  CHECK_EQ((Fixed::min() / Fixed::one()).raw(), INT32_MIN);
  // Identity and self-division.
  CHECK_EQ((Fixed::from_raw(-777) / Fixed::one()).raw(), -777);
  CHECK_EQ((Fixed::from_raw(-777) / Fixed::from_raw(-777)).raw(), 65536);
  CHECK_EQ((Fixed::min() / Fixed::min()).raw(), 65536);
}

void test_min_max_clamp_compare() {
  Fixed a = Fixed::from_int(-2), b = Fixed::from_int(3);
  CHECK(a < b && b > a && a <= a && a == a && a != b);
  CHECK(fp::min(a, b) == a && fp::max(a, b) == b);
  CHECK(fp::clamp(Fixed::from_int(5), a, b) == b);
  CHECK(fp::clamp(Fixed::from_int(-5), a, b) == a);
  CHECK(fp::clamp(Fixed::from_int(1), a, b) == Fixed::from_int(1));
}

void test_edges_all_pairs() {
  auto vals = edge_values();
  for (auto a : vals)
    for (auto b : vals) check_pair(a, b);
}

void test_random() {
  fp_test::Rng rng(0x5eed1234abcdULL);
  for (int i = 0; i < 300000; ++i) {
    std::uint64_t r = rng.next();
    // Mix full-range values with small ones so both overflow and rounding bite.
    std::int32_t a = static_cast<std::int32_t>(r);
    std::int32_t b = static_cast<std::int32_t>(r >> 32);
    if (i % 3 == 1) a >>= 14;
    if (i % 3 == 2) b >>= 12;
    check_pair(a, b);
  }
}

void test_wide() {
  static_assert(Wide::from_fixed(Fixed::one()).raw() == (std::int64_t{1} << 32));
  // The motivating case: 200 * 200 = 40000 overflows Fixed but not Wide.
  Fixed d = Fixed::from_int(200);
  Wide sq = fp::wide_mul(d, d);
  CHECK(sq.to_double() == 40000.0);
  CHECK(sq == Wide::from_double(40000.0));
  CHECK_EQ(sq.narrow().raw(), INT32_MAX);                     // narrowing saturates
  CHECK_EQ((fp::wide_mul(d, -d)).narrow().raw(), INT32_MIN);
  CHECK(fp::wide_mul(Fixed::from_double(1.5), Fixed::from_double(-2.0)).to_double() == -3.0);
  // Exact: no rounding in a single product.
  CHECK_EQ(fp::wide_mul(Fixed::from_raw(1), Fixed::from_raw(1)).raw(), 1);
  CHECK(fp::wide_mul(Fixed::from_raw(1), Fixed::from_raw(1)) > Wide::zero());
  // Largest single products.
  CHECK_EQ(fp::wide_mul(Fixed::min(), Fixed::min()).raw(), std::int64_t{1} << 62);
  CHECK_EQ(fp::wide_mul(Fixed::min(), Fixed::max()).raw(),
           static_cast<long long>(I128(INT32_MIN) * INT32_MAX));
  // A sum of two largest products is 2^63: Wide addition saturates.
  Wide big = fp::wide_mul(Fixed::min(), Fixed::min());
  CHECK(big + big == Wide::max());
  CHECK(-big + -big == Wide::min());
  CHECK(-big + -big - big == Wide::min());
  CHECK(big - -big == Wide::max());
  CHECK((big - big).raw() == 0);
  CHECK(Wide::max() + Wide::from_raw(1) == Wide::max());
  CHECK(Wide::min() + Wide::from_raw(-1) == Wide::min());
  CHECK(Wide::min() - Wide::from_raw(1) == Wide::min());
  CHECK(Wide::max() - Wide::from_raw(-1) == Wide::max());
  CHECK(Wide::max() + Wide::min() == Wide::from_raw(-1));
  CHECK(-Wide::min() == Wide::max());
  CHECK(fp::abs(Wide::min()) == Wide::max());
  CHECK(fp::abs(Wide::from_raw(-9)) == Wide::from_raw(9));

  // narrow(): nearest, ties toward +infinity, saturating at both ends.
  CHECK_EQ(Wide::from_raw(0x8000).narrow().raw(), 1);    // +0.5 raw
  CHECK_EQ(Wide::from_raw(-0x8000).narrow().raw(), 0);   // -0.5 raw
  CHECK_EQ(Wide::from_raw(0x7fff).narrow().raw(), 0);
  CHECK_EQ(Wide::from_raw(-0x8001).narrow().raw(), -1);
  CHECK_EQ(Wide::max().narrow().raw(), INT32_MAX);
  CHECK_EQ(Wide::min().narrow().raw(), INT32_MIN);
  CHECK_EQ(Wide::from_fixed(Fixed::from_raw(-12345)).narrow().raw(), -12345);

  // from_double/to_double edges.
  CHECK(Wide::from_double(0.5 / 4294967296.0).raw() == 1);
  CHECK(Wide::from_double(-0.5 / 4294967296.0).raw() == -1);
  CHECK(Wide::from_double(1e30) == Wide::max());
  CHECK(Wide::from_double(-1e30) == Wide::min());
  CHECK(Wide::from_double(std::numeric_limits<double>::quiet_NaN()).raw() == 0);
  CHECK(Wide::from_double(2147483648.0) == Wide::max());
  CHECK(Wide::from_double(-2147483648.0) == Wide::min());
}

// Squared distance between two positions at large coordinates, the case Wide
// exists for: the exact result, never a silent saturation.
void test_large_coordinate_products() {
  const std::int32_t coords[] = {INT32_MIN, -65536 * 32767, -65536 * 200, -1, 0, 1, 65536 * 200,
                                 65536 * 32767, INT32_MAX};
  for (auto x : coords)
    for (auto y : coords) {
      Fixed fx = Fixed::from_raw(x), fy = Fixed::from_raw(y);
      I128 exact = I128(x) * y;
      CHECK_EQ(fp::wide_mul(fx, fy).raw(), static_cast<long long>(exact));
      Wide sum = fp::wide_mul(fx, fx) + fp::wide_mul(fy, fy);  // x^2 + y^2
      I128 e2 = I128(x) * x + I128(y) * y;
      I128 want = e2 > INT64_MAX ? I128(INT64_MAX) : e2;
      CHECK_EQ(sum.raw(), static_cast<long long>(want));
    }
}

}  // namespace

int main() {
  test_constants_and_conversion();
  test_add_sub_neg();
  test_mul();
  test_div();
  test_min_max_clamp_compare();
  test_edges_all_pairs();
  test_random();
  test_wide();
  test_large_coordinate_products();
  return fp_test::finish();
}
