# fighter-physics: the owner's brief

The owner's request, 2026-10-18 (lightly edited for reading), made while
starting ledgefall, an online successor to lost-footing:

> I'd like to create another project for a from-scratch parallelized game
> engine to handle the physics, and it should allow for different entities to
> have various coefficients for all types of accelerations and speeds. I want
> to emulate Super Smash Bros in allowing for customized dash accelerations,
> top speeds, aerial accelerations and top speeds, etc.

## Fixed points

- **From scratch.** No third-party physics engine. The standard library is
  the only required dependency.
- **Parallel.** The step uses every core it is given, through the engine's
  own job system.
- **Deterministic.** The same inputs produce bit-identical state on every
  platform, compiler and thread count. The first user, ledgefall, runs the
  same simulation on an arm64 Mac client and an x86_64 Linux server, and
  rolls it back and re-simulates it, so results must not depend on floating
  point behaviour or scheduling order.
- **Per-entity movement coefficients, Smash style.** Every entity carries its
  own parameters for every kind of acceleration and speed: walk, dash, run,
  traction, air acceleration and top speed, air friction, gravity, fall and
  fast-fall speeds, jump velocities and counts, and similar. Changing a
  character's feel is data, not code.
- **Built for platform fighters.** Platforms that crumble, respawn, move,
  can be passed through from below, or can be destroyed; placeable objects
  that change mobility (walls, slick and sticky patches, springs); and
  vertical-only body contact between players, as lost-footing does it.
- **Rollback-ready.** State can be snapshotted, restored and hashed cheaply.
- **A library.** No rendering, input, audio or networking: ledgefall's client,
  its server and future games link it.
