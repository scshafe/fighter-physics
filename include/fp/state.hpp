#pragma once
// Rollback support for a World: a stable 64-bit hash of the whole simulation
// state. The hash feeds every field, in a fixed order and as little-endian
// integers, so it is the same on every platform and compiler, and it covers
// exactly what `restore` brings back (config is not state).

#include <cstdint>

#include "fp/world.hpp"

namespace fp {

std::uint64_t hash(const World& w);

}  // namespace fp
