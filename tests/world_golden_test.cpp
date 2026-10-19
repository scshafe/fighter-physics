// Golden traces of entity 0 in a World with scripted inputs and placements: the exact state
// after every frame and the frame's events, compared with tests/golden/<name>.txt. Raw
// Q16.16 integers. Regenerate with FP_UPDATE_GOLDEN=1 and review the diff.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>

#include "world_fixture.hpp"

using namespace fp;
using namespace fixture;

namespace {

const char* mode_name(Mode m) {
  static const char* names[] = {"Idle", "Walk", "Dash", "Run", "Skid", "Turnaround", "JumpSquat", "Airborne"};
  return names[static_cast<int>(m)];
}

const char* event_name(EventType t) {
  static const char* names[] = {"Landed",          "Fell",           "Eliminated",       "LandedOnEntity",
                                "BumpedEntity",    "PlatformShaking", "PlatformCrumbled", "PlatformRespawned",
                                "PlatformDestroyed", "PlatformDoomed", "PlaceableRemoved"};
  return names[static_cast<int>(t)];
}

// `script(frame, world, intent)` runs before each step (1-based frame): it may place things
// and sets the intent.
std::string trace(Config cfg, EntityState start, int frames,
                  const std::function<void(int, World&, Intent&)>& script) {
  std::ostringstream out;
  out << "# frame mode facing pos.x pos.y vel.x vel.y support (raw Q16.16); then the frame's events\n";
  World w = make_world(cfg);
  w.entities.push_back(start);
  Events ev;
  for (int f = 1; f <= frames; ++f) {
    Intent in;
    script(f, w, in);
    ev.clear();
    step(cfg, w, std::span<const Intent>(&in, 1), ev);
    const EntityState& e = w.entities[0];
    out << f << ' ' << mode_name(e.mode) << ' ' << static_cast<int>(e.facing) << ' ' << e.pos.x.raw() << ' '
        << e.pos.y.raw() << ' ' << e.vel.x.raw() << ' ' << e.vel.y.raw() << ' ' << e.support;
    for (const Event& x : ev) out << ' ' << event_name(x.type) << ':' << x.id << ':' << x.other;
    out << '\n';
  }
  return out.str();
}

void check_golden(const std::string& name, const std::string& got) {
  const std::string path = std::string(FP_GOLDEN_DIR) + "/" + name + ".txt";
  if (std::getenv("FP_UPDATE_GOLDEN") != nullptr) {
    std::ofstream(path) << got;
    std::printf("wrote %s\n", path.c_str());
    return;
  }
  std::ifstream in(path);
  if (!in) {
    std::fprintf(stderr, "%s: missing golden file (run with FP_UPDATE_GOLDEN=1)\n", path.c_str());
    ++fp_test::failures();
    return;
  }
  std::stringstream want;
  want << in.rdbuf();
  if (want.str() == got) return;
  std::istringstream a(want.str()), b(got);
  std::string la, lb;
  for (int line = 1;; ++line) {
    bool ha = static_cast<bool>(std::getline(a, la)), hb = static_cast<bool>(std::getline(b, lb));
    if (!ha && !hb) break;
    if (!ha || !hb || la != lb) {
      std::fprintf(stderr, "%s line %d differs:\n  want: %s\n  got:  %s\n", name.c_str(), line,
                   ha ? la.c_str() : "<end>", hb ? lb.c_str() : "<end>");
      break;
    }
  }
  ++fp_test::failures();
}

Placeable block(double l, double r, double b, double t, int life = 0) {
  Placeable p;
  p.rect = {F(l), F(r), F(b), F(t)};
  p.life = life;
  return p;
}

// The default movement parameters on a real floor [-200, 200] x [-10, 0]; wide blast zone.
Config floor_stage() {
  Config c;
  c.stage.platforms = {solid(-200, 200, -10, 0)};
  c.stage.blast = {F(-300), F(300), F(-100), F(200)};
  c.params.assign(1, MovementParams{});
  return c;
}

}  // namespace

int main() {
  // Run right; a wall appears ahead at frame 10 and stops the run; it expires at frame 90 and the
  // run goes on through where it was.
  {
    Config c = floor_stage();
    check_golden("wall_blocks_run", trace(c, spawn_on(c, 0, F(0)), 120, [](int f, World& w, Intent& in) {
                   if (f == 10) place(w, block(20, 24, 0, 6, 80));
                   in.move_x = F(1);
                 }));
  }
  // Fall onto a wall, stand on it until it expires (frame 40), fall to the floor and land.
  {
    Config c = floor_stage();
    EntityState e;
    e.mode = Mode::Airborne;
    e.pos = Vec2::from_double(22, 20);
    check_golden("wall_land_expire", trace(c, e, 70, [](int f, World& w, Intent&) {
                   if (f == 1) place(w, block(20, 24, 0, 6, 40));
                 }));
  }
  // Run, stop and slide across ice [60, 120] (traction x0.25) and then mud [120, 190] (traction
  // x3, speeds x0.5) on the floor: the release at frame 40 slides far on the ice and stops fast in the mud.
  {
    Config c = floor_stage();
    auto patch = [](PlaceableKind k, double l, double r, double traction, double speed) {
      Placeable p = block(l, r, -1, 0);
      p.kind = k;
      p.traction_scale = F(traction);
      p.speed_scale = F(speed);
      return p;
    };
    check_golden("ice_then_mud", trace(c, spawn_on(c, 0, F(0)), 160, [&](int f, World& w, Intent& in) {
                   if (f == 1) {
                     place(w, patch(PlaceableKind::Slick, 60, 120, 0.25, 1.0));
                     place(w, patch(PlaceableKind::Sticky, 120, 190, 3.0, 0.5));
                   }
                   in.move_x = f <= 40 ? F(1) : F(0);
                 }));
  }
  return fp_test::finish();
}
