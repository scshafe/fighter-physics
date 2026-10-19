// The cross-platform determinism check: the busy scenario's state hash every 25
// frames, committed in tests/golden/determinism_hashes.txt. CI runs this on Linux
// (gcc and clang) and macOS (clang, arm64) and in Debug and Release; every one
// must reproduce the file, so a divergence names the first frame at which a
// platform's simulation differs. Regenerate with FP_UPDATE_GOLDEN=1 after an
// intended change to the simulation, and review the diff.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "scenario.hpp"

int main() {
  const std::string path = std::string(FP_GOLDEN_DIR) + "/determinism_hashes.txt";
  scenario::Scenario sc(48);
  std::ostringstream out;
  out << "# frame state-hash of the 48-entity scenario (tests/scenario.hpp), every 25 frames\n";
  for (int f = 1; f <= 1500; ++f) {
    sc.advance(sc.next_intents(), nullptr);
    if (f % 25 == 0) {
      char line[64];
      std::snprintf(line, sizeof line, "%d %016llx\n", f, static_cast<unsigned long long>(fp::hash(sc.w)));
      out << line;
    }
  }
  const std::string got = out.str();
  if (std::getenv("FP_UPDATE_GOLDEN") != nullptr) {
    std::ofstream(path) << got;
    std::printf("wrote %s\n", path.c_str());
    return 0;
  }
  std::ifstream in(path);
  std::stringstream want;
  want << in.rdbuf();
  if (want.str() != got) {
    std::istringstream a(want.str()), b(got);
    std::string la, lb;
    while (std::getline(a, la) && std::getline(b, lb)) {
      if (la != lb) {
        std::fprintf(stderr, "determinism trace differs:\n  want: %s\n  got:  %s\n", la.c_str(), lb.c_str());
        break;
      }
    }
    ++fp_test::failures();
  }
  return fp_test::finish();
}
