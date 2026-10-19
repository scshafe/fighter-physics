#pragma once
// A 2D vector of Fixed. Sums, differences and scalings stay Q16.16 and saturate
// per component. Any product of two positions or vectors (dot, cross, squared
// length or distance) returns Wide (Q32.32), so e.g. the squared distance of
// two points 200 units apart (40,000, beyond Q16.16's 32,768) is exact.
// Narrow explicitly (Wide::narrow) only when the result is known to fit.

#include "fp/fixed.hpp"

namespace fp {

struct Vec2 {
  Fixed x{};
  Fixed y{};

  constexpr Vec2() noexcept = default;
  constexpr Vec2(Fixed x_, Fixed y_) noexcept : x(x_), y(y_) {}

  // Edge conversion (see Fixed::from_double).
  static constexpr Vec2 from_double(double x, double y) noexcept {
    return {Fixed::from_double(x), Fixed::from_double(y)};
  }

  friend constexpr bool operator==(const Vec2&, const Vec2&) noexcept = default;

  friend constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
  friend constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
  friend constexpr Vec2 operator-(Vec2 a) noexcept { return {-a.x, -a.y}; }
  // Scaling by a Fixed scalar (each component rounds and saturates as Fixed*Fixed).
  friend constexpr Vec2 operator*(Vec2 a, Fixed s) noexcept { return {a.x * s, a.y * s}; }
  friend constexpr Vec2 operator*(Fixed s, Vec2 a) noexcept { return {s * a.x, s * a.y}; }
  friend constexpr Vec2 operator/(Vec2 a, Fixed s) noexcept { return {a.x / s, a.y / s}; }

  constexpr Vec2& operator+=(Vec2 o) noexcept { return *this = *this + o; }
  constexpr Vec2& operator-=(Vec2 o) noexcept { return *this = *this - o; }
  constexpr Vec2& operator*=(Fixed s) noexcept { return *this = *this * s; }
  constexpr Vec2& operator/=(Fixed s) noexcept { return *this = *this / s; }
};

// Products of two vectors: Wide, exact per term; the sum of the two terms
// saturates, which can only happen for dot and length_squared at -2^31 * -2^31
// twice (2^63). cross cannot overflow Wide.
constexpr Wide dot(Vec2 a, Vec2 b) noexcept { return wide_mul(a.x, b.x) + wide_mul(a.y, b.y); }
// z component of the 3D cross product: a.x*b.y - a.y*b.x.
constexpr Wide cross(Vec2 a, Vec2 b) noexcept { return wide_mul(a.x, b.y) - wide_mul(a.y, b.x); }
constexpr Wide length_squared(Vec2 a) noexcept { return dot(a, a); }
// The difference of two positions saturates per component before it is squared.
constexpr Wide distance_squared(Vec2 a, Vec2 b) noexcept { return length_squared(a - b); }

constexpr Vec2 abs(Vec2 a) noexcept { return {abs(a.x), abs(a.y)}; }
constexpr Vec2 min(Vec2 a, Vec2 b) noexcept { return {min(a.x, b.x), min(a.y, b.y)}; }
constexpr Vec2 max(Vec2 a, Vec2 b) noexcept { return {max(a.x, b.x), max(a.y, b.y)}; }
constexpr Vec2 clamp(Vec2 v, Vec2 lo, Vec2 hi) noexcept {
  return {clamp(v.x, lo.x, hi.x), clamp(v.y, lo.y, hi.y)};
}

}  // namespace fp
