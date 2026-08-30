# Numerical methods — integrator validation

This directory is the numerical-analysis half of the project: a Python harness
that establishes what "correct" means before any of it gets ported to C++.

The question it answers is one the original prototype couldn't. That version
integrated the Earth's orbit with explicit Euler and produced a plot that looked
entirely convincing — a clean circle, right radius, right period. It was also
wrong, and nothing about the picture said so. What was needed was a number
instead of a picture.

Three measured claims come out of it:

1. Explicit Euler is **first order**, and its energy error **grows without bound**.
2. Velocity Verlet is **second order**, and its energy error **stays in a fixed band forever**.
3. RK4 is **fourth order** and far more accurate per step, but its error **drifts** — slowly, and without limit.

Doing this in Python rather than C++ was deliberate. Establishing what correct
looks like is exploratory work, and it belongs in a language where an experiment
costs seconds. The C++ engine is then validated against these exact trajectories
rather than against its own assumptions — see `export_reference.py`.

## Layout

| File | What it does |
|------|-------------|
| `nbody.py` | Force law and the conserved quantities (energy, angular momentum) |
| `integrators.py` | Euler, velocity Verlet, RK4, behind one shared interface |
| `scenarios.py` | Initial conditions with known analytic answers |
| `simulate.py` | Driver — runs a scenario, records trajectory and diagnostics |
| `make_figures.py` | The four figures |
| `export_reference.py` | Writes the trajectory the C++ suite is checked against |

The integrators share a single signature, `step(positions, velocities, masses,
dt) -> (positions, velocities)`, which is what lets the driver and the test
suite treat them as interchangeable. The C++ engine uses the same contract.

```bash
./venv/bin/pytest                              # 34 tests
./venv/bin/python -m validation.make_figures   # writes figures/
```

## How correctness is established

The central test is `TestConvergenceOrder::test_measured_order_matches_theory`.

Global error scales as `C · dt^p`, where `p` is the method's order. Taking logs
makes that a straight line, so fitting a slope to (log dt, log error) recovers
`p` directly. The three methods measure **0.98, 2.00, and 4.10** against
theoretical 1, 2, and 4.

This matters more than any trajectory plot. A picture of an orbit can only show
that the answer is plausible. Measuring that an integrator converges at its
theoretical rate shows that it is *right*, and localizes the bug when it isn't:
a Verlet implementation that measures order 1 has used `a_n` where it needed
`a_{n+1}`, and an RK4 that measures 2 has a stage evaluated at the wrong state.

Two things that make the measurement subtle:

**The timestep range matters.** Error only scales as `dt^p` inside the
asymptotic regime. Euler at `dt=1e-2` has an error of ~2 AU — larger than the
orbit itself — and measures order 0.83 rather than 1. Each method therefore gets
its own dt range in both the tests and the figures.

**Not every scenario can measure everything.** `test_particle` gives the
orbiting body zero mass so the central body stays pinned and the analytic
solution is exact — but energy and angular momentum are mass-weighted sums, so
both are identically zero there and their relative drift divides by zero. It's
for convergence only; conservation work uses `sun_earth` or `eccentric`.

## Scenarios

| Name | Purpose |
|------|---------|
| `sun_earth` | Circular 1 AU orbit. Conservation diagnostics. |
| `test_particle` | Massless body, fixed Sun, exact analytic solution. Convergence only. |
| `eccentric` | e≈0.36 ellipse. The case that actually stresses an integrator. |

The eccentric case is the one that earns its place. A circular orbit is
deceptively easy — speed and force magnitude never change, so a single fixed
timestep suits the entire orbit. On an ellipse the body moves fastest exactly
where the force is strongest, so a timestep that is comfortable at aphelion is
far too coarse at perihelion. Real trajectories — comets, transfer orbits,
elliptical satellite orbits — are eccentric.

## Figures

- **`energy_drift.png`** — Euler climbing off the top of the chart, Verlet
  oscillating inside a flat band, RK4 creeping upward and crossing above
  Verlet's band around year 20.
- **`convergence_order.png`** — log-log error against dt, with measured slopes.
- **`orbit_shapes.png`** — trajectories on a shared axis. Euler spirals from
  1.0 to 2.38 AU over 50 years; Verlet and RK4 each draw one clean circle.
- **`equal_cost.png`** — comparison at equal *force evaluations* rather than
  equal dt, since RK4 spends four per step and Verlet two.

That last figure has the result worth reading carefully. Given a budget of
200,000 force evaluations over 200 orbits, **RK4 wins outright** — roughly 7e-6
relative error against Verlet's 1e-4. The symplectic method is not simply
better; at this budget the fourth-order method is more accurate even after
paying for its extra force evaluations.

The distinction is in the shape rather than the ranking. Verlet's error envelope
is flat: 200 orbits in, it is what it was at orbit 1, and it will still be that
at orbit 200,000. RK4's climbs steadily, so the crossover sits somewhere in the
thousands of orbits. Which method is correct to reach for is therefore entirely
a question of integration horizon — a five-year mission propagation wants RK4;
a Gyr-scale galaxy simulation can only use the flat line.
