# fighter-physics

A from-scratch, deterministic, parallel 2D physics and movement engine for
platform fighters, with Super Smash Bros-style per-entity movement
coefficients: walk, dash, run, traction, air acceleration and top speed,
gravity, fall and fast-fall, and jumps. Its first user is
[ledgefall](https://github.com/scshafe/ledgefall).

- Brief: [docs/brief.md](docs/brief.md). Design: [docs/design.md](docs/design.md).
- C++20, CMake, no third-party dependencies.

## Build and test

```sh
./scripts/verify      # configure, build and run the tests (CMake + CTest)
```

## License

MIT (`LICENSE`).
