# physics-sims

A gravitational simulation engine written from scratch in C++17 — numerical
integrators validated against conservation laws, with a real-time OpenGL
renderer on top and a Python harness that cross-checks the physics.

![Energy conservation over 100 years](figures/energy_drift.png)

> **Status:** in active development. The orbital scene runs; the N-body and
> black-hole scenes are next. See [Scenes](#scenes).

---

## Why this exists

A simulated orbit that *looks* right is very easy to produce and tells you
almost nothing. This project started as a forward-Euler two-body integrator that
drew a clean circle at the correct radius with the correct period — and was
silently wrong. It injects energy on every step, and over fifty simulated years
the Earth spirals from 1.0 out to 2.38 AU. Nothing in the picture says so.

So the organizing principle is measurement. Every claim the simulation makes is
backed by a quantity that either holds or doesn't: energy, angular momentum, and
the measured order of convergence.

## Architecture

One engine, several scenes — not several unrelated demos.

```
core/          integrators, N-body solver, conserved quantities   (no OpenGL)
render/        window, shaders, camera, buffers                   (no physics)
scenes/        the simulations themselves
validation/    Python harness — integrator comparison, energy plots
tests/         pytest suite + a dependency-free C++ suite
vendor/        glad (generated GL loader)
archive/       the original Python prototype this grew out of
```

`core/` links no graphics libraries at all. That constraint is what keeps the
physics testable without opening a window, and what lets a second scene reuse
the first one's work instead of duplicating it.

## Scenes

| Scene | Status | What it demonstrates |
|-------|--------|---------------------|
| **Orbital mechanics** | running | Sun–Earth orbits, integrator comparison at runtime; J2 oblateness and ground tracks in progress |
| **N-body cluster** | planned | Barnes-Hut octree, O(n²) → O(n log n), instanced rendering at 10k+ bodies |
| **Black hole** | planned | Geodesic raytracing in a fragment shader, gravitational lensing |

Each forces exactly one new hard problem: integrator correctness, then
algorithms and performance, then the GPU.

## Numerical methods

Three integrators, implemented and compared rather than assumed:

| Method | Order | Force evals/step | Long-run energy behavior |
|--------|-------|------------------|-------------------------|
| Explicit Euler | 1 | 1 | Grows without bound |
| Velocity Verlet | 2 | 2 | Bounded oscillation (symplectic) |
| RK4 | 4 | 4 | Slow monotonic drift |

Correctness is established by measuring each method's **empirical order of
convergence** — fitting the slope of log(error) against log(dt), which recovers
the theoretical order — rather than by eyeballing trajectories. The three
measure 0.98, 2.00, and 4.10 against theoretical 1, 2, and 4.

The C++ engine is additionally **cross-validated against the Python harness**:
both integrate the same scenario at the same timestep, and the trajectories are
required to agree to 1e-12 AU. They currently agree to **7.4e-14 AU** over 730
steps — around 11 metres across a 150-million-kilometre orbit. Two independent
implementations in two languages matching to that precision is a considerably
stronger correctness argument than either test suite passing alone.

The result worth reporting is that "which integrator is best" has no
context-free answer. At equal computational cost over 200 orbits, RK4 is roughly
20× more accurate than Verlet — the symplectic method does not simply win. But
Verlet's error is *flat* while RK4's climbs, so past a few thousand orbits the
ranking inverts. A five-year mission propagation wants RK4; a Gyr-scale
astrophysical run can only use the flat line.

[`validation/README.md`](validation/README.md) covers the numerical-methods work
in full.

## Building and running

```bash
brew install cmake glfw glm

cmake -S . -B build && cmake --build build -j8
./build/orbital        # the scene
./build/core_tests     # the engine test suite
```

Press `1`, `2`, `3` in the window to switch integrators at runtime and watch
Euler's orbit visibly decay against the other two. Full controls and a
sanitizer-build recipe are in [`BUILDING.md`](BUILDING.md).

The Python harness:

```bash
python3 -m venv venv && ./venv/bin/pip install numpy matplotlib pytest
./venv/bin/pytest                              # 34 tests
./venv/bin/python -m validation.make_figures   # writes figures/
```

## Units

AU, solar masses, years — which makes `G = 4π²` by Kepler's third law, and
Earth's orbital velocity exactly 2π AU/yr. Scaled units keep the numbers near 1
and preserve floating-point precision; SI would put positions around 1e11 and
masses around 1e30 in the same expression, spending a large share of a double's
15–16 significant digits on exponent range that carries no information.
