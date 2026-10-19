#include "fp/state.hpp"

#include <cstring>

#include "archive.hpp"

namespace fp {
namespace {

// A free slot is the default Placeable, so nothing stale reaches the hash.
bool is_free_slot(const Placeable& p) {
  const Placeable z{};
  return p.kind == z.kind && p.life == 0 && p.rect.left == z.rect.left && p.rect.right == z.rect.right &&
         p.rect.bottom == z.rect.bottom && p.rect.top == z.rect.top && p.traction_scale == z.traction_scale &&
         p.speed_scale == z.speed_scale;
}

// FNV-1a over little-endian bytes, with a final mix.
class Hasher {
 public:
  template <class T>
  void num(const T& x) {
    if constexpr (std::is_enum_v<T>) {
      num(static_cast<std::underlying_type_t<T>>(x));
    } else {
      using U = std::make_unsigned_t<T>;
      U u = static_cast<U>(x);
      for (std::size_t i = 0; i < sizeof(U); ++i) {
        byte(static_cast<std::uint8_t>(u & 0xff));
        u = static_cast<U>(u >> 8);
      }
    }
  }
  void fx(const Fixed& x) { num(x.raw()); }
  template <class C>
  void count(const C& v) {
    num(static_cast<std::uint32_t>(v.size()));
  }
  std::uint64_t finish() const {
    std::uint64_t z = h_;
    z ^= z >> 33;
    z *= 0xff51afd7ed558ccdULL;
    z ^= z >> 33;
    z *= 0xc4ceb9fe1a85ec53ULL;
    return z ^ (z >> 33);
  }

 private:
  void byte(std::uint8_t b) {
    h_ ^= b;
    h_ *= 0x100000001b3ULL;
  }
  std::uint64_t h_ = 0xcbf29ce484222325ULL;
};

// Appends little-endian bytes.
class Writer {
 public:
  explicit Writer(std::vector<std::uint8_t>& out) : out_(out) {}
  template <class T>
  void num(const T& x) {
    if constexpr (std::is_enum_v<T>) {
      num(static_cast<std::underlying_type_t<T>>(x));
    } else {
      using U = std::make_unsigned_t<T>;
      U u = static_cast<U>(x);
      for (std::size_t i = 0; i < sizeof(U); ++i) {
        out_.push_back(static_cast<std::uint8_t>(u & 0xff));
        u = static_cast<U>(u >> 8);
      }
    }
  }
  void fx(const Fixed& x) { num(x.raw()); }
  template <class C>
  void count(const C& v) {
    num(static_cast<std::uint32_t>(v.size()));
  }

 private:
  std::vector<std::uint8_t>& out_;
};

// Reads little-endian bytes; after any shortfall `ok` is false and values are 0.
class Reader {
 public:
  explicit Reader(std::span<const std::uint8_t> in) : in_(in) {}
  template <class T>
  void num(T& x) {
    if constexpr (std::is_enum_v<T>) {
      std::underlying_type_t<T> raw{};
      num(raw);
      x = static_cast<T>(raw);
    } else {
      using U = std::make_unsigned_t<T>;
      U u = 0;
      if (in_.size() - pos_ < sizeof(U)) {
        ok = false;
      } else {
        for (std::size_t i = 0; i < sizeof(U); ++i) u = static_cast<U>(u | (static_cast<U>(in_[pos_ + i]) << (8 * i)));
        pos_ += sizeof(U);
      }
      x = static_cast<T>(u);
    }
  }
  void fx(Fixed& x) {
    std::int32_t raw = 0;
    num(raw);
    x = Fixed::from_raw(raw);
  }
  // Every element takes at least one byte, so a count larger than what is left
  // is malformed (and must not be allocated).
  template <class C>
  void count(C& v) {
    std::uint32_t n = 0;
    num(n);
    if (n > in_.size() - pos_) {
      ok = false;
      n = 0;
    }
    v.assign(n, typename C::value_type{});
  }
  bool at_end() const { return pos_ == in_.size(); }
  bool ok = true;

 private:
  std::span<const std::uint8_t> in_;
  std::size_t pos_ = 0;
};

constexpr std::uint8_t kMagic[4] = {'F', 'P', 'W', 4};  // format, then version

bool flag(std::uint8_t v) { return v <= 1; }

// Semantic checks on a parsed world.
bool valid(const Config& cfg, const World& w) {
  if (w.entities.size() != cfg.params.size() || w.platforms.size() != cfg.stage.platforms.size()) return false;
  const int np = static_cast<int>(w.platforms.size());
  const int ne = static_cast<int>(w.entities.size());
  for (const PlatformState& p : w.platforms) {
    if (static_cast<std::uint8_t>(p.phase) > static_cast<std::uint8_t>(PlatformPhase::Destroyed)) return false;
    if (p.timer < 0 || p.clock < 0) return false;
  }
  if (w.placeables.size() > static_cast<std::size_t>(kMaxPlaceables)) return false;
  for (const Placeable& p : w.placeables) {
    if (static_cast<std::uint8_t>(p.kind) > static_cast<std::uint8_t>(PlaceableKind::Sticky)) return false;
    if (!flag(p.active) || p.life < 0) return false;
    if (p.active != 0 && !(p.rect.left < p.rect.right && p.rect.bottom < p.rect.top)) return false;
    if (p.traction_scale < Fixed::zero() || p.speed_scale < Fixed::zero()) return false;
    if (p.active == 0 && !is_free_slot(p)) return false;
  }
  const int nslots = np + static_cast<int>(w.placeables.size());
  for (const EntityState& e : w.entities) {
    if (static_cast<std::uint8_t>(e.mode) > static_cast<std::uint8_t>(Mode::Airborne)) return false;
    if (e.facing != 1 && e.facing != -1) return false;
    if (e.prev_dir < -1 || e.prev_dir > 1) return false;
    if (!flag(e.prev_jump) || !flag(e.fast_falling) || !flag(e.prev_down) || !flag(e.eliminated)) return false;
    if (e.support < -1 || e.support >= nslots || e.support_entity < -1 || e.support_entity >= ne) return false;
    if (e.frames < 0 || e.jumps_used < 0 || e.drop_timer < 0) return false;
  }
  return true;
}

}  // namespace

void snapshot(const World& w, std::vector<std::uint8_t>& out) {
  out.assign(std::begin(kMagic), std::end(kMagic));
  Writer wr(out);
  detail::io_world(wr, w);
}

bool restore(const Config& cfg, World& w, std::span<const std::uint8_t> bytes) {
  if (bytes.size() < sizeof(kMagic) || std::memcmp(bytes.data(), kMagic, sizeof(kMagic)) != 0) return false;
  Reader r(bytes.subspan(sizeof(kMagic)));
  World parsed;
  detail::io_world(r, parsed);
  if (!r.ok || !r.at_end() || !valid(cfg, parsed)) return false;
  w = std::move(parsed);
  return true;
}

std::uint64_t hash(const World& w) {
  Hasher h;
  detail::io_world(h, w);
  return h.finish();
}

}  // namespace fp
