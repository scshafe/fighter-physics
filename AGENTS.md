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

<!-- scshafe-dev:begin landing -->
## Verify and landing

Managed by scshafe-dev: `dev adopt` and `dev update` refresh this section from `dev.toml`; change `dev.toml`, not these lines.

Before finishing, both of these must pass:

```sh
./scripts/verify
dev check .
```

How a change lands:

1. Work on a branch and open a PR.
2. Run the two commands above. If the repository is private, GitHub Actions does not run for it: verify locally and say in the PR what you ran. If it is public, wait for CI to be green.
3. Merge your own PR with a merge commit, one change at a time: `gh pr merge <N> --merge --subject "Merge #<N>: <title>"`. Never squash or rebase (both are off on the repository), and pass `--subject`: `gh pr merge` does not make the `Merge #N: <title>` subject by itself.

The project's agent may merge its own PR and push `main`; there is no approval gate.
<!-- scshafe-dev:end landing -->
