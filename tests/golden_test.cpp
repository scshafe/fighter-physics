// Golden traces: a scripted input sequence -> the exact state after every
// frame, compared with tests/golden/<name>.txt. Raw Q16.16 integers, so any
// change to the numbers (or to the platform's arithmetic) shows as a diff.
//
// After an intended behaviour change, regenerate with FP_UPDATE_GOLDEN=1
// (`FP_UPDATE_GOLDEN=1 ./build/fighter_physics_golden_test`), and review the diff.
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "fp/movement.hpp"

using namespace fp;

namespace {

struct Seg {
  int frames;
  double x;
  bool jump = false;
  bool fast_fall = false;
};

const char* mode_name(Mode m) {
  switch (m) {
    case Mode::Idle: return "Idle";
    case Mode::Walk: return "Walk";
    case Mode::Dash: return "Dash";
    case Mode::Run: return "Run";
    case Mode::Skid: return "Skid";
    case Mode::Turnaround: return "Turnaround";
    case Mode::JumpSquat: return "JumpSquat";
    case Mode::Airborne: return "Airborne";
  }
  return "?";
}

std::string trace(const std::vector<Seg>& script, const MovementParams& p, const Ground& g) {
  std::ostringstream out;
  out << "# frame mode facing pos.x pos.y vel.x vel.y (raw Q16.16)\n";
  EntityState e;
  e.pos.y = g.y;
  int frame = 0;
  for (const Seg& s : script) {
    for (int i = 0; i < s.frames; ++i) {
      step(e, Intent{Fixed::from_double(s.x), s.jump, s.fast_fall}, p, g);
      out << ++frame << ' ' << mode_name(e.mode) << ' ' << static_cast<int>(e.facing) << ' ' << e.pos.x.raw()
          << ' ' << e.pos.y.raw() << ' ' << e.vel.x.raw() << ' ' << e.vel.y.raw() << '\n';
    }
  }
  return out.str();
}

void check_golden(const std::string& name, const std::vector<Seg>& script) {
  const std::string path = std::string(FP_GOLDEN_DIR) + "/" + name + ".txt";
  const std::string got = trace(script, MovementParams{}, Ground{Fixed::zero()});
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

}  // namespace

int main() {
  // Walk right at half tilt, stop, flick into a dash and run, release into a skid.
  check_golden("ground_basics", {{20, 0.5}, {20, 0.0}, {40, 1.0}, {30, 0.0}});

  // Dash and run right, then run turnaround to the left, then stop.
  check_golden("run_turnaround", {{30, 1.0}, {30, -1.0}, {25, 0.0}});

  // Full hop (button held through the squat), drift right, aerial jump while
  // pushing left, fast-fall, land.
  check_golden("fullhop_aerial_fastfall",
               {{8, 0.0, true}, {12, 1.0, true}, {10, 1.0, false}, {10, -1.0, true}, {10, -1.0, false},
                {30, -1.0, false, true}, {10, 0.0}});

  // Short hop (button released during the squat), with no input.
  check_golden("shorthop", {{2, 0.0, true}, {40, 0.0, false}, {5, 0.0}});

  // Jump out of a run, steering in the air and landing.
  check_golden("runjump", {{30, 1.0}, {7, 1.0, true}, {60, 1.0}, {20, 0.0}});

  return fp_test::finish();
}
