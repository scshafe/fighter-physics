#pragma once
// A stage of platforms and the world state of the entities on it.
//
// Config (the Stage and each entity's MovementParams) never changes during a
// match; World is all the simulation state and is plain data, so it can be
// copied, snapshotted and hashed. Entity ids are indices into World::entities
// and stay valid for the whole match.

#include <cstdint>
#include <span>
#include <vector>

#include "fp/movement.hpp"

namespace fp {

struct Rect {
  Fixed left{}, right{}, bottom{}, top{};  // bottom <= top, y up
};

enum class PlatformKind : std::uint8_t {
  Solid,        // blocks from every side: floor, ceiling and walls
  PassThrough,  // stood on from above only; entities rise through it and can drop through it
};

struct PlatformDef {
  PlatformKind kind = PlatformKind::Solid;
  Rect rect{};  // pass-through platforms use only `top`, `left` and `right`
};

struct Stage {
  std::vector<PlatformDef> platforms;
  Rect blast{};  // an entity whose position leaves this rectangle is eliminated
};

// Everything a game configures and the step reads but never changes.
struct Config {
  Stage stage;
  std::vector<MovementParams> params;  // one per entity, by id
};

enum class EventType : std::uint8_t {
  Landed,      // id: entity, other: platform it landed on
  Fell,        // id: entity left its support (walked off, drop-through, platform vanished)
  Eliminated,  // id: entity left the blast zone
};

struct Event {
  EventType type;
  std::int32_t id;
  std::int32_t other;
};

using Events = std::vector<Event>;

struct World {
  std::vector<EntityState> entities;
  std::uint32_t frame = 0;
};

// An entity at rest on top of a platform, centred at x.
EntityState spawn_on(const Config& cfg, int platform, Fixed x);

// Advance the world one frame: motion, then contacts, then events (which
// replace the contents of `events`). `intents` has one entry per entity.
void step(const Config& cfg, World& w, std::span<const Intent> intents, Events& events);

}  // namespace fp
