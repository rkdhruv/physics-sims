# Solar System Sim — archived prototype

The original Python two-body simulation this project grew out of, kept as a
reference point. Superseded by `core/` (C++ engine) and `validation/` (Python
harness).

It integrates the Sun–Earth system with explicit Euler and plots the trajectory.
The output looks correct — a circle at 1 AU with a one-year period — and is
wrong in a way the plot cannot show: Euler adds energy every step, so the orbit
spirals outward without bound.

Diagnosing that is what the rest of the project is built on. `figures/` in the
repo root has the measured version of the story; `Figure_1.png` here is the
original qualitative plot.

Kept unchanged, typos and all, as the starting point.
