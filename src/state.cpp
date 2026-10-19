#include "fp/state.hpp"

#include "archive.hpp"

namespace fp {
namespace {

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

}  // namespace

std::uint64_t hash(const World& w) {
  Hasher h;
  detail::io_world(h, w);
  return h.finish();
}

}  // namespace fp
