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

class JobSystem;

struct Rect {
  Fixed left{}, right{}, bottom{}, top{};  // bottom <= top, y up
};

enum class PlatformKind : std::uint8_t {
  Solid,        // blocks from every side: floor, ceiling and walls
  PassThrough,  // stood on from above only; entities rise through it and can drop through it
};

struct PlatformStress {
  Fixed shake_at{}, doom_at{};
  Fixed still_rate{};   // per entity standing on it with |vel.x| < still_speed
  Fixed moving_rate{};  // per entity standing on it at or above that speed
  Fixed still_speed{};
  Fixed rider_share{};  // of a rider's rate that reaches the platform below a stack, per head
  Fixed doom_rate{};
  Fixed decay{};        // per frame while nothing stands on it
  Fixed straddle_share{};  // of its rate each platform gets from an entity on several; 0 = 1/N
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

  // Crumbling by accumulated stress (instead of by a first touch): when
  // `stress.shake_at` > 0 the platform carries a stress in [0, 1] that the
  // entities on it raise every frame, and `shake_frames` is not used.
  //   stress < shake_at           Solid
  //   shake_at <= stress < doom_at  Shaking (still solid; falls back to Solid if the stress decays)
  //   doom_at <= stress < 1         Doomed (still solid; rises by `doom_rate` a frame by itself)
  //   stress >= 1                   Gone for `respawn_frames` (0 = for good), stress back to 0
  // All rates are per frame.
  PlatformStress stress;
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

// Things the game puts on the stage during a match. They live in the world
// (plain data, in the snapshot and the hash), in numbered slots that stay
// valid until the placeable goes. A slot is also a platform id for contacts:
// platform ids from `stage.platforms.size()` up are placeable slots, so
// `Landed.other` and `EntityState::support` name a wall by
// `stage.platforms.size() + slot`.
enum class PlaceableKind : std::uint8_t {
  Wall,    // a solid block: stood on, walked into, bumped from below, like a Solid platform
  Slick,   // a patch of ground: friction scaled by `traction_scale` (ice: below 1)
  Sticky,  // a patch of ground: friction scaled up and ground speed caps by `speed_scale` (mud: above 1, below 1)
  Spring,  // a pad: launches an entity that is on the ground with its feet in the rect
  OneWay,  // a ledge: stood on from above only, like a PassThrough platform (drop-through included),
           // at the rect's top and x-range; optionally only for one entity
};

constexpr int kMaxPlaceables = 64;

struct Placeable {
  PlaceableKind kind = PlaceableKind::Wall;
  std::uint8_t active = 0;  // 0: the slot is free (and the rest of it is zero)
  Rect rect{};
  std::int32_t life = 0;  // frames left; 0 while active: permanent

  // Patches (Slick, Sticky). An entity standing on the ground with its feet in `rect`
  // (left <= x <= right, bottom <= y <= top) moves with `traction` and `skid_traction`
  // times `traction_scale` and `walk_max`, `dash_max` and `run_max` times `speed_scale`.
  // Both are >= 0; overlapping patches multiply, in slot order. Slick and Sticky act
  // the same way and differ only in the label the game sees (and its typical values).
  Fixed traction_scale = Fixed::one();
  Fixed speed_scale = Fixed::one();

  // Spring: an entity on the ground (any grounded mode) with its feet in the rect, after its
  // own motion and contacts that frame, is put in the air with vel.y = launch.y and vel.x
  // increased by launch.x; `Sprung` (id: entity, other: slot). Its aerial jumps are untouched.
  // Only the lowest-numbered spring under the feet acts. launch.y must be > 0 (so the
  // entity leaves the pad and does not spring again until it lands).
  Vec2 launch{};

  // OneWay: -1 = everyone lands on it; an entity id = only that entity does, and every other entity
  // passes through it as if it were not there. Must be < the number of entities (`restore` checks).
  std::int32_t only_for = -1;
};

enum class EventType : std::uint8_t {
  Landed,             // id: entity, other: platform it landed on
  Fell,               // id: entity left its support (walked off, drop-through, platform vanished)
  Eliminated,         // id: entity left the blast zone
  LandedOnEntity,     // id: entity landed on the head of entity `other`
  BumpedEntity,       // id: entity rising hit the underside of entity `other`
  PlatformShaking,    // id: platform started to crumble
  PlatformCrumbled,   // id: platform is gone
  PlatformRespawned,  // id: platform is solid again
  PlatformDestroyed,  // id: platform was destroyed (permanently)
  PlatformDoomed,     // id: a stress platform passed `doom_at`
  PlaceableRemoved,   // id: placeable slot, expired or removed
  Sprung,             // id: entity launched by the spring in slot `other`
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
  Doomed,     // in play, stress keeps rising until it falls
};

struct PlatformState {
  PlatformPhase phase = PlatformPhase::Solid;
  std::int32_t timer = 0;  // frames left in Shaking or Gone
  std::int32_t clock = 0;  // position in the motion cycle
  Vec2 offset{};           // current displacement from the rest position
  Fixed stress{};          // 0..1, platforms with `PlatformDef::stress`
  // Conveyor: units a frame, signed (+x), set by the game between steps (0: not a belt). Each
  // frame, after its own motion, a grounded entity whose support is this platform, and the
  // entities stacked on its head, is moved by `belt` along x (stopped by walls, and carried off
  // an edge). The platform does not move. Ground friction is unchanged.
  Fixed belt{};
};

struct World {
  std::vector<EntityState> entities;
  std::vector<PlatformState> platforms;  // one per Stage platform
  std::vector<Placeable> placeables;     // slots, at most kMaxPlaceables
  std::uint32_t frame = 0;
};

// A world with every platform in play and no entities yet.
World make_world(const Config& cfg);

// An entity at rest on top of a platform, centred at x.
EntityState spawn_on(const Config& cfg, int platform, Fixed x);

// Put a placeable on the stage in the lowest free slot (a new one if none is
// free); returns the slot, or -1 if all kMaxPlaceables slots are taken or the
// rect is empty or inverted, only_for is below -1, a scale is negative or a spring does not launch upward. `p.life` > 0 makes it expire after that many
// frames. An entity it lands on is pushed out the shallowest way on the next step.
int place(World& w, Placeable p);

// Take a placeable away now (its riders fall). No effect on a free slot.
void remove_placeable(const Config& cfg, World& w, int slot, Events& events);

// Destroy a platform for good (a move, a hazard): entities standing on it fall.
// Appends to `events`; the caller owns clearing it.
void destroy_platform(const Config& cfg, World& w, int platform, Events& events);

// A one-off increase of a stress platform's stress (a hard landing, a stomp),
// which a negative amount lowers; clamps to [0, 1] and applies the phase
// changes at once, so 1 or more crumbles it. No effect on platforms without
// a stress model, or that are Gone or Destroyed. Appends to `events`.
void add_stress(const Config& cfg, World& w, int platform, Fixed amount, Events& events);

// Advance the world one frame: per entity motion and contacts, then the
// platforms (crumble, respawn, motion, carrying their riders). Appends the
// frame's events to `events` in a deterministic order; the caller clears it.
// `intents` has one entry per entity.
//
// With a job system the per-entity phases run across its workers; the result
// (state and events) is identical to the serial step for any worker count.
void step(const Config& cfg, World& w, std::span<const Intent> intents, Events& events,
          JobSystem* jobs = nullptr);

}  // namespace fp
