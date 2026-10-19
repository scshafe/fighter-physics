#pragma once
// The one list of World's fields. An "archive" visits them in order: Hasher
// (const World) feeds them to a hash, and other archives write or read bytes.
// Adding a field to the state means adding it here, once.

#include <cstdint>
#include <type_traits>
#include <vector>

#include "fp/world.hpp"

namespace fp::detail {

// Each archive provides:
//   num(x)    an integer or enum, as its own width
//   fx(x)     a Fixed, as its raw int32
//   count(v)  the length of a vector (a loading archive resizes it)
template <class A, class V>
void io(A& a, V& v) {  // Vec2 (const or not)
  a.fx(v.x);
  a.fx(v.y);
}

template <class A, class E>
void io_entity(A& a, E& e) {
  io(a, e.pos);
  io(a, e.vel);
  a.num(e.mode);
  a.num(e.facing);
  a.num(e.prev_dir);
  a.num(e.flick_age);
  a.num(e.frames);
  a.num(e.jumps_used);
  a.num(e.prev_jump);
  a.num(e.fast_falling);
  a.num(e.prev_down);
  a.num(e.eliminated);
  a.num(e.support);
  a.num(e.support_entity);
  a.num(e.drop_timer);
}

template <class A, class P>
void io_platform(A& a, P& p) {
  a.num(p.phase);
  a.num(p.timer);
  a.num(p.clock);
  io(a, p.offset);
  a.fx(p.stress);
}

template <class A, class R>
void io_rect(A& a, R& r) {
  a.fx(r.left);
  a.fx(r.right);
  a.fx(r.bottom);
  a.fx(r.top);
}

template <class A, class P>
void io_placeable(A& a, P& p) {
  a.num(p.kind);
  a.num(p.active);
  io_rect(a, p.rect);
  a.num(p.life);
}

template <class A, class W>
void io_world(A& a, W& w) {
  a.num(w.frame);
  a.count(w.platforms);
  for (auto& p : w.platforms) io_platform(a, p);
  a.count(w.placeables);
  for (auto& p : w.placeables) io_placeable(a, p);
  a.count(w.entities);
  for (auto& e : w.entities) io_entity(a, e);
}

}  // namespace fp::detail
