# 0001: Number representation

Status: accepted, 2026-10-19.

## Context

The engine must give bit-identical state on Linux x86_64 and macOS arm64, for
any worker count, because ledgefall runs the same simulation on an arm64 Mac
client and an x86_64 Linux server and rolls it back and re-simulates it
(`docs/brief.md`). Any divergence, however small, desynchronises a match.

The platforms are macOS clang and Linux gcc/clang. MSVC is not a target, so
`__int128` is available.

## Options

**A. Strict IEEE 754 floats.** Possible only with discipline that nothing
enforces: FMA contraction off (`-ffp-contract=off`) on every target, no
`-ffast-math`, no libm (`sin`, `sqrt`, `pow` differ between libraries), no
x87, and the same flags in every consumer's build (ledgefall, netplay-kit,
the Swift app). arm64 contracts to FMA by default in clang; one consumer
forgetting the flag silently breaks rollback, and the failure shows up as a
desync in a live match. Compilers may also reassociate under some flags.

**B. Fixed-point integers.** Integer arithmetic is fully specified by the
language (with overflow handled explicitly), so results depend on neither
compiler flags, optimiser nor CPU. The costs are a smaller dynamic range,
explicit rounding and overflow rules, and integer algorithms for anything
beyond `+ - * /` (sqrt, trig) when they are needed.

## Decision

Fixed-point, with floats only at the edges.

- **`fp::Fixed`: Q16.16** in an `int32_t` (resolution 2^-16, about 1.5e-5;
  range [-32768, 32768)). Used for all simulation state: positions,
  velocities, accelerations, parameters. One unit is roughly one Smash unit,
  time is in frames.
- **`fp::Wide`: Q32.32** in an `int64_t` (range about +-2^31), the result type
  of any product of two `Fixed` quantities that is not itself a Q16.16
  quantity: `Vec2` dot, cross and `length_squared`, squared distances and the
  like. The raw product of two Q16.16 values is exactly a Q32.32 value, so a
  single product is exact and never rounds or overflows (|raw| < 2^31 each,
  so |product| <= 2^62). A sum of two products can reach 2^63, so `Wide`
  addition saturates. Two positions 200 units apart give a squared distance of
  about 40,000, beyond Q16.16's 32,768: this is why such products must not
  return `Fixed`. Converting `Wide` back to `Fixed` is explicit, rounds, and
  saturates.
- **Q32.32 as the general scalar was considered** and rejected as the
  default: it doubles the state size that is snapshotted and hashed every
  frame for rollback, and the range (+-2^31 units) is far more than a stage
  needs. It is not rejected for portability: `__int128` makes a wide multiply
  available on every target platform, and the implementation and the tests
  use it as the reference.
- **Rounding:** `Fixed * Fixed` and `Fixed / Fixed` round to nearest, ties
  toward +infinity (add half, arithmetic shift right). Narrowing `Wide` to
  `Fixed` uses the same rule. Conversions from `double` round to nearest,
  ties away from zero, and are the only place rounding mode matters.
- **Overflow:** `+`, `-`, unary `-`, `*`, `abs` and narrowing saturate at the
  representable min/max (`min` is `-2^31` raw; `-min` and `abs(min)` give
  `max`). Division by zero saturates by the sign of the numerator, and 0/0 is
  0. Nothing is undefined behaviour and nothing depends on the platform.
- **Floats only at the edges:** `from_double`/`to_double` (and `Vec2`
  equivalents) exist for authoring parameters, rendering and tools. No
  simulation code path uses floating point. The library is built so that
  this can be grepped for.
- **Shifts:** right shifts of signed values are arithmetic (guaranteed since
  C++20).

## Consequences

- Determinism does not depend on consumers' compiler flags.
- Parameters are authored as fixed-point values (`Fixed::from_double(0.08)`
  is `constexpr`).
- Square roots, trigonometry and vector normalisation, if ever needed, are
  integer algorithms added later and covered by their own tests.
- The cross-platform hash test (design milestone 6) is the backstop for the
  claim in this record.
