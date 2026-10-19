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
  Rect rect{};  // the rest position; pass-through platforms use only `top`, `left`, `right`

  // Motion: the platform goes from its rest position to rest + travel and back,
  // linearly, in `move_period` frames (even; 0 = does not move). It carries
  // the entities standing on it.
  Vec2 travel{};
  std::int32_t move_period = 0;

  // Crumbling: once an entity has stood on it, it shakes for `shake_frames`
  // (still solid), then is gone for `respawn_frames`, then solid again.
  // shake_frames = 0: never crumbles. respawn_frames = 0: stays gone.
  std::int32_t shake_frames = 0;
  std::int32_t respawn_frames = 0;
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
  Landed,             // id: entity, other: platform it landed on
  Fell,               // id: entity left its support (walked off, drop-through, platform vanished)
  Eliminated,         // id: entity left the blast zone
  PlatformShaking,    // id: platform started to crumble
  PlatformCrumbled,   // id: platform is gone
  PlatformRespawned,  // id: platform is solid again
  PlatformDestroyed,  // id: platform was destroyed (permanently)
};

struct Event {
  EventType type;
  std::int32_t id;     // an entity id or a platform id, by type
  std::int32_t other;  // see EventType; -1 if unused
};

using Events = std::vector<Event>;

enum class PlatformPhase : std::uint8_t {
  Solid,      // in play
  Shaking,    // in play, about to crumble
  Gone,       // crumbled, may respawn
  Destroyed,  // permanently removed
};

struct PlatformState {
  PlatformPhase phase = PlatformPhase::Solid;
  std::int32_t timer = 0;  // frames left in Shaking or Gone
  std::int32_t clock = 0;  // position in the motion cycle
  Vec2 offset{};           // current displacement from the rest position
};

struct World {
  std::vector<EntityState> entities;
  std::vector<PlatformState> platforms;  // one per Stage platform
  std::uint32_t frame = 0;
};

// A world with every platform in play and no entities yet.
World make_world(const Config& cfg);

// An entity at rest on top of a platform, centred at x.
EntityState spawn_on(const Config& cfg, int platform, Fixed x);

// Destroy a platform for good (a move, a hazard): entities standing on it fall.
// Appends to `events`; the caller owns clearing it.
void destroy_platform(const Config& cfg, World& w, int platform, Events& events);

// Advance the world one frame: per entity motion and contacts, then the
// platforms (crumble, respawn, motion, carrying their riders). Appends the
// frame's events to `events` in a deterministic order; the caller clears it.
// `intents` has one entry per entity.
void step(const Config& cfg, World& w, std::span<const Intent> intents, Events& events);

}  // namespace fp
