#pragma once
// Rollback support for a World: a stable 64-bit hash of the whole simulation
// state. The hash feeds every field, in a fixed order and as little-endian
// integers, so it is the same on every platform and compiler, and it covers
// exactly what `restore` brings back (config is not state).

#include <cstdint>
#include <span>
#include <vector>

#include "fp/world.hpp"

namespace fp {

std::uint64_t hash(const World& w);

// Write the whole simulation state to `out` (replacing its contents) as a
// platform-independent byte string: a 4-byte header, then every field of
// archive.hpp as little-endian integers. The size depends only on the number of
// entities and platforms (about 40 bytes per entity).
void snapshot(const World& w, std::vector<std::uint8_t>& out);

// Replace `w` with the state in `bytes`. Returns false, leaving `w` untouched,
// if the bytes are not a well-formed snapshot of a world for this `cfg`: wrong
// header, truncated or trailing bytes, a different number of entities or
// platforms than the config has, or a field out of range (enum, facing, flag,
// support index). Everything the snapshot does not hold (the config) stays as
// it is, so a restored world steps exactly like the one that was saved.
bool restore(const Config& cfg, World& w, std::span<const std::uint8_t> bytes);

}  // namespace fp
