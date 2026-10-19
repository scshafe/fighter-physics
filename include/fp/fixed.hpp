#pragma once
// The number types of the simulation (docs/decisions/0001-number-representation.md).
//
//   Fixed  Q16.16 in an int32_t: positions, velocities, accelerations, parameters.
//   Wide   Q32.32 in an int64_t: the result of a product of two Fixed quantities
//          (dot, cross, squared distance), which Fixed could not hold.
//
// Every operation is defined for every input: arithmetic saturates, division by
// zero saturates by the numerator's sign, and nothing is undefined behaviour or
// depends on the platform. Floating point appears only in from_double/to_double.

#include <compare>
#include <cstdint>

namespace fp {

namespace detail {
// Reference-quality 128-bit integer; MSVC is not a target.
__extension__ typedef __int128 i128;

constexpr std::int32_t kI32Max = INT32_MAX;
constexpr std::int32_t kI32Min = INT32_MIN;

constexpr std::int32_t saturate_i32(std::int64_t v) noexcept {
  return v > kI32Max ? kI32Max : (v < kI32Min ? kI32Min : static_cast<std::int32_t>(v));
}
constexpr std::int32_t saturate_i32(i128 v) noexcept {
  return v > kI32Max ? kI32Max : (v < kI32Min ? kI32Min : static_cast<std::int32_t>(v));
}

// floor(n / d) for d != 0; the C++ `/` truncates.
constexpr std::int64_t floor_div(std::int64_t n, std::int64_t d) noexcept {
  std::int64_t q = n / d;
  std::int64_t r = n % d;
  if (r != 0 && ((r < 0) != (d < 0))) --q;
  return q;
}

// Round a double to the nearest integer, ties away from zero, saturating to
// [lo, hi]; NaN gives 0. Exact: the truncation and the fraction are both exact
// doubles, so no intermediate rounds.
constexpr std::int64_t round_double(double x, std::int64_t lo, std::int64_t hi) noexcept {
  if (x != x) return 0;
  if (x >= 9.2233720368547758e18) return hi;
  if (x <= -9.2233720368547758e18) return lo;
  std::int64_t t = static_cast<std::int64_t>(x);  // truncates toward zero
  double frac = x - static_cast<double>(t);
  if (frac >= 0.5) ++t;
  else if (frac <= -0.5) --t;
  return t > hi ? hi : (t < lo ? lo : t);
}
}  // namespace detail

class Wide;

// Q16.16 fixed point. Range [-32768, 32768), resolution 2^-16.
class Fixed {
 public:
  static constexpr int kFracBits = 16;
  static constexpr std::int32_t kOneRaw = 1 << kFracBits;

  constexpr Fixed() noexcept = default;

  static constexpr Fixed from_raw(std::int32_t raw) noexcept {
    Fixed f;
    f.raw_ = raw;
    return f;
  }
  // Whole units; saturates outside [-32768, 32767].
  static constexpr Fixed from_int(std::int32_t v) noexcept {
    return from_raw(detail::saturate_i32(static_cast<std::int64_t>(v) * kOneRaw));
  }
  // Edge conversion: nearest, ties away from zero, saturating; NaN gives 0.
  static constexpr Fixed from_double(double v) noexcept {
    return from_raw(static_cast<std::int32_t>(
        detail::round_double(v * kOneRaw, detail::kI32Min, detail::kI32Max)));
  }

  static constexpr Fixed zero() noexcept { return from_raw(0); }
  static constexpr Fixed one() noexcept { return from_raw(kOneRaw); }
  static constexpr Fixed epsilon() noexcept { return from_raw(1); }
  static constexpr Fixed max() noexcept { return from_raw(detail::kI32Max); }
  static constexpr Fixed min() noexcept { return from_raw(detail::kI32Min); }

  constexpr std::int32_t raw() const noexcept { return raw_; }
  constexpr double to_double() const noexcept { return static_cast<double>(raw_) / kOneRaw; }
  // Largest whole number <= this (floor).
  constexpr std::int32_t floor_int() const noexcept { return raw_ >> kFracBits; }

  friend constexpr bool operator==(Fixed, Fixed) noexcept = default;
  friend constexpr auto operator<=>(Fixed, Fixed) noexcept = default;

  friend constexpr Fixed operator+(Fixed a, Fixed b) noexcept {
    return from_raw(detail::saturate_i32(static_cast<std::int64_t>(a.raw_) + b.raw_));
  }
  friend constexpr Fixed operator-(Fixed a, Fixed b) noexcept {
    return from_raw(detail::saturate_i32(static_cast<std::int64_t>(a.raw_) - b.raw_));
  }
  friend constexpr Fixed operator-(Fixed a) noexcept {
    return from_raw(detail::saturate_i32(-static_cast<std::int64_t>(a.raw_)));
  }
  // Nearest, ties toward +infinity; saturating.
  friend constexpr Fixed operator*(Fixed a, Fixed b) noexcept {
    std::int64_t p = static_cast<std::int64_t>(a.raw_) * b.raw_;
    return from_raw(detail::saturate_i32((p + (kOneRaw >> 1)) >> kFracBits));
  }
  // Nearest, ties toward +infinity; saturating. x/0 saturates by the sign of x
  // (0/0 is 0).
  friend constexpr Fixed operator/(Fixed a, Fixed b) noexcept {
    if (b.raw_ == 0) {
      return a.raw_ > 0 ? max() : (a.raw_ < 0 ? min() : zero());
    }
    std::int64_t n = static_cast<std::int64_t>(a.raw_) * kOneRaw;  // |n| <= 2^47
    std::int64_t d = b.raw_;
    if (d < 0) {
      n = -n;
      d = -d;
    }
    // floor(n/d + 1/2)
    return from_raw(detail::saturate_i32(detail::floor_div(2 * n + d, 2 * d)));
  }

  constexpr Fixed& operator+=(Fixed o) noexcept { return *this = *this + o; }
  constexpr Fixed& operator-=(Fixed o) noexcept { return *this = *this - o; }
  constexpr Fixed& operator*=(Fixed o) noexcept { return *this = *this * o; }
  constexpr Fixed& operator/=(Fixed o) noexcept { return *this = *this / o; }

 private:
  std::int32_t raw_ = 0;
};

constexpr Fixed abs(Fixed a) noexcept { return a < Fixed::zero() ? -a : a; }
constexpr Fixed min(Fixed a, Fixed b) noexcept { return b < a ? b : a; }
constexpr Fixed max(Fixed a, Fixed b) noexcept { return a < b ? b : a; }
// lo <= hi is the caller's contract; if not, lo wins.
constexpr Fixed clamp(Fixed v, Fixed lo, Fixed hi) noexcept {
  return v > hi ? (hi < lo ? lo : hi) : (v < lo ? lo : v);
}

// Q32.32 fixed point: range about +-2^31, resolution 2^-32. Addition and
// subtraction saturate; a single product of two Fixed (wide_mul) is exact.
class Wide {
 public:
  static constexpr int kFracBits = 32;

  constexpr Wide() noexcept = default;

  static constexpr Wide from_raw(std::int64_t raw) noexcept {
    Wide w;
    w.raw_ = raw;
    return w;
  }
  static constexpr Wide from_fixed(Fixed f) noexcept {
    return from_raw(static_cast<std::int64_t>(f.raw()) * (std::int64_t{1} << 16));
  }
  // Edge conversion: nearest, ties away from zero, saturating; NaN gives 0.
  static constexpr Wide from_double(double v) noexcept {
    return from_raw(detail::round_double(v * 4294967296.0, INT64_MIN, INT64_MAX));
  }

  static constexpr Wide zero() noexcept { return from_raw(0); }
  static constexpr Wide max() noexcept { return from_raw(INT64_MAX); }
  static constexpr Wide min() noexcept { return from_raw(INT64_MIN); }

  constexpr std::int64_t raw() const noexcept { return raw_; }
  constexpr double to_double() const noexcept { return static_cast<double>(raw_) / 4294967296.0; }

  // Back to Q16.16: nearest, ties toward +infinity, saturating.
  constexpr Fixed narrow() const noexcept {
    return Fixed::from_raw(detail::saturate_i32((static_cast<detail::i128>(raw_) + (1 << 15)) >> 16));
  }

  friend constexpr bool operator==(Wide, Wide) noexcept = default;
  friend constexpr auto operator<=>(Wide, Wide) noexcept = default;

  friend constexpr Wide operator+(Wide a, Wide b) noexcept {
    std::int64_t r;
    if (__builtin_add_overflow(a.raw_, b.raw_, &r)) return b.raw_ > 0 ? max() : min();
    return from_raw(r);
  }
  friend constexpr Wide operator-(Wide a, Wide b) noexcept {
    std::int64_t r;
    if (__builtin_sub_overflow(a.raw_, b.raw_, &r)) return b.raw_ < 0 ? max() : min();
    return from_raw(r);
  }
  friend constexpr Wide operator-(Wide a) noexcept {
    return a.raw_ == INT64_MIN ? max() : from_raw(-a.raw_);
  }
  constexpr Wide& operator+=(Wide o) noexcept { return *this = *this + o; }
  constexpr Wide& operator-=(Wide o) noexcept { return *this = *this - o; }

 private:
  std::int64_t raw_ = 0;
};

constexpr Wide abs(Wide a) noexcept { return a < Wide::zero() ? -a : a; }

// The exact product of two Fixed as a Wide: |raw a * raw b| <= 2^62, so it
// neither rounds nor overflows.
constexpr Wide wide_mul(Fixed a, Fixed b) noexcept {
  return Wide::from_raw(static_cast<std::int64_t>(a.raw()) * b.raw());
}

}  // namespace fp
