# fighter-physics agent notes

- A C++20 library: a deterministic, parallel 2D physics and movement engine
  for platform fighters (`docs/brief.md`, fixed; `docs/design.md`, the
  designer's, kept in step with the code). No rendering, input, audio,
  networking or game rules: those belong to ledgefall and netplay-kit.
- Determinism is the contract. The same inputs must give bit-identical state
  on Linux x86_64 and macOS arm64, for any worker count. Every change that
  touches the step keeps the hash-equality tests green. A hard-to-reverse
  choice, such as the number representation, gets a record in `docs/decisions/`.
- No third-party dependencies in the library. Tests use plain CTest
  executables unless a decision says otherwise.
- Public, MIT. CI runs on GitHub: wait for it before merging.
