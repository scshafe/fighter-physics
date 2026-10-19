// The skeleton's one test: the library links and reports its version.
#include <cstdio>
#include <cstring>

#include "fp/version.hpp"

int main() {
  if (std::strcmp(fp::version(), "0.0.1") != 0) {
    std::fprintf(stderr, "unexpected version %s\n", fp::version());
    return 1;
  }
  return 0;
}
