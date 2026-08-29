"""Generate the integrator-comparison figures into figures/.

    ./venv/bin/python -m validation.make_figures          # write PNGs
    ./venv/bin/python -m validation.make_figures --show   # also display them
"""

import argparse
import pathlib

import matplotlib
import numpy as np

from validation.integrators import EXPECTED_ORDER, FORCE_EVALS_PER_STEP
from validation.scenarios import eccentric, sun_earth, test_particle
from validation.simulate import analytic_circular_position, integrate_only, run

FIGURES = pathlib.Path(__file__).resolve().parent.parent / "figures"

# Colorblind-safe palette, fixed per integrator so the hues mean the same thing
# in every figure.
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_SECONDARY = "#52514e"
INK_MUTED = "#8a8985"

COLORS = {
    "euler": "#2a78d6",
    "verlet": "#eb6834",
    "rk4": "#1baf7a",
}

LABELS = {
    "euler": "Explicit Euler",
    "verlet": "Velocity Verlet",
    "rk4": "RK4",
}

ORDER = ["euler", "verlet", "rk4"]


def _style():
    matplotlib.rcParams.update({
        "figure.facecolor": SURFACE,
        "axes.facecolor": SURFACE,
        "savefig.facecolor": SURFACE,
        "axes.edgecolor": INK_MUTED,
        "axes.labelcolor": INK_SECONDARY,
        "axes.titlecolor": INK,
        "axes.linewidth": 0.8,
        "axes.spines.top": False,
        "axes.spines.right": False,
        "xtick.color": INK_SECONDARY,
        "ytick.color": INK_SECONDARY,
        "xtick.labelsize": 9,
        "ytick.labelsize": 9,
        "grid.color": "#e4e3df",
        "grid.linewidth": 0.8,
        "font.size": 10,
        "figure.dpi": 140,
        "savefig.bbox": "tight",
    })


def _direct_label(ax, x, y, text, color, dx=10, dy=0):
    """Label a series in place: a colored dot carries the identity, the text
    stays in neutral ink."""
    ax.plot([x], [y], marker="o", markersize=6, color=color,
            markeredgecolor=SURFACE, markeredgewidth=1.5, zorder=5, clip_on=False)
    ax.annotate(text, (x, y), textcoords="offset points", xytext=(dx, dy),
                va="center", ha="left", fontsize=9, color=INK, clip_on=False)


def _place_labels(ax, entries, min_sep_px=15, base_dx=10):
    """Direct-label several series, nudging apart any labels that collide.

    Only the text moves; the colored dot stays at the true data position.

    entries: list of (x, y, text, color).
    """
    fig = ax.figure
    fig.canvas.draw()  # transforms aren't valid until the first draw

    pixels = [ax.transData.transform((x, y))[1] for x, y, _, _ in entries]

    adjusted = {}
    previous = None
    for i in sorted(range(len(entries)), key=lambda k: pixels[k]):
        y = pixels[i]
        if previous is not None and y - previous < min_sep_px:
            y = previous + min_sep_px
        adjusted[i] = y
        previous = y

    for i, (x, y, text, color) in enumerate(entries):
        dy_points = (adjusted[i] - pixels[i]) * 72.0 / fig.dpi
        _direct_label(ax, x, y, text, color, dx=base_dx, dy=dy_points)


def _band_top(values, fraction=0.05):
    """Upper edge of the last `fraction` of a series.

    Verlet's energy error swings across five orders of magnitude per orbit, so
    the final sample is a poor label anchor -- a trough would claim 1e-15 for a
    band sitting near 1e-10. The local maximum is what the eye reads as the band.
    """
    window = max(1, int(len(values) * fraction))
    return np.max(values[-window:])


def _envelope(times, values, bins=200):
    """Per-bin maximum, collapsing a dense oscillating band into one line.

    160k raw samples of an oscillating error render as a solid block of color
    that hides every other series; the upper envelope stays legible.
    """
    edges = np.linspace(times[0], times[-1], bins + 1)
    idx = np.clip(np.digitize(times, edges) - 1, 0, bins - 1)

    centers, peaks = [], []
    for b in range(bins):
        mask = idx == b
        if np.any(mask):
            centers.append(0.5 * (edges[b] + edges[b + 1]))
            peaks.append(np.max(values[mask]))
    return np.array(centers), np.array(peaks)


def energy_drift(duration=100.0, dt=1e-3):
    """Relative energy error against time for all three integrators.

    Log y-axis: the methods differ by ten orders of magnitude.
    """
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(8, 4.6))

    labels = []
    for name in ORDER:
        result = run(sun_earth(), name, dt=dt, duration=duration)
        drift = result.energy_drift

        # drift[0] is exactly 0 by construction; log scale can't show it.
        nonzero = drift > 0
        times = result.times[nonzero]
        values = drift[nonzero]

        ax.plot(times, values, color=COLORS[name], linewidth=2,
                solid_capstyle="round", zorder=3)
        labels.append((times[-1], _band_top(values), LABELS[name], COLORS[name]))

    ax.set_yscale("log")
    ax.set_xlabel("time (years)")
    ax.set_ylabel("relative energy error  |ΔE / E₀|")
    ax.set_title(
        f"Energy conservation over {duration:.0f} years  (Sun–Earth, dt = {dt} yr)",
        fontsize=11, pad=14, loc="left",
    )
    ax.grid(axis="y", zorder=0)
    ax.set_axisbelow(True)
    ax.margins(x=0.02)
    # Headroom on the right so the direct labels don't run off the canvas.
    ax.set_xlim(right=duration * 1.16)

    _place_labels(ax, labels)  # after the limits are final; depends on transforms

    fig.text(0.01, -0.04,
             "Euler's error grows without bound. Verlet's oscillates inside a fixed band — "
             "the defining property of a symplectic method.",
             fontsize=8.5, color=INK_SECONDARY)

    _save(fig, "energy_drift.png")
    return fig


def convergence_order():
    """Log-log error against dt, where each line's slope is the method's order."""
    import matplotlib.pyplot as plt

    # Per-method dt ranges, matching the test suite, chosen to stay in the
    # asymptotic regime. Euler at dt=1e-2 has an error larger than the orbit.
    dt_sets = {
        "euler": [1e-3, 5e-4, 2.5e-4, 1.25e-4, 6.25e-5],
        "verlet": [1e-2, 5e-3, 2.5e-3, 1.25e-3, 6.25e-4],
        "rk4": [1e-2, 5e-3, 2.5e-3, 1.25e-3, 6.25e-4],
    }

    fig, ax = plt.subplots(figsize=(7.2, 5))
    exact = analytic_circular_position(1.0)

    for name in ORDER:
        dts = dt_sets[name]
        errors = []
        for dt in dts:
            positions, _ = integrate_only(test_particle(), name, dt, 1.0)
            errors.append(np.linalg.norm(positions[1] - exact))

        slope, _ = np.polyfit(np.log(dts), np.log(errors), 1)

        ax.plot(dts, errors, color=COLORS[name], linewidth=2, marker="o",
                markersize=5, markeredgecolor=SURFACE, markeredgewidth=1.2, zorder=3)
        # Labelled above the largest-dt end, where the descending line leaves
        # clear space.
        _direct_label(ax, dts[0], errors[0],
                      f"{LABELS[name]}  ·  measured order {slope:.2f}", COLORS[name],
                      dx=8, dy=14)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.invert_xaxis()
    ax.set_xlabel("timestep dt (years)")
    ax.set_ylabel("position error after 1 orbit (AU)")
    ax.set_title("Order of accuracy: error vs. timestep", fontsize=11, pad=14, loc="left")
    ax.grid(zorder=0)
    ax.set_axisbelow(True)

    expected = ", ".join(f"{LABELS[n]} {EXPECTED_ORDER[n]}" for n in ORDER)
    fig.text(0.01, -0.03,
             f"Slope on a log-log plot equals the method's order. Theory: {expected}.",
             fontsize=8.5, color=INK_SECONDARY)

    _save(fig, "convergence_order.png")
    return fig


def orbit_shapes(duration=50.0, dt=1e-3):
    """Trajectories side by side on a shared axis."""
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(1, 3, figsize=(11, 3.9))

    runs = {name: run(test_particle(), name, dt=dt, duration=duration)
            for name in ORDER}

    # Shared scale across the panels: with per-panel autoscaling, Euler's
    # 2.4 AU spiral and Verlet's 1.0 AU circle both fill their axes and read
    # as the same size.
    extent = max(
        np.max(np.hypot(r.trajectory[:, 1, 0], r.trajectory[:, 1, 1]))
        for r in runs.values()
    ) * 1.1

    for ax, name in zip(axes, ORDER):
        result = runs[name]
        xs = result.trajectory[:, 1, 0]
        ys = result.trajectory[:, 1, 1]

        ax.plot(xs, ys, color=COLORS[name], linewidth=0.5, alpha=0.85, zorder=3)
        ax.plot([0], [0], marker="o", markersize=7, color=INK_MUTED, zorder=4)

        radii = np.hypot(xs, ys)
        ax.set_title(
            f"{LABELS[name]}\nfinal radius {radii[-1]:.4f} AU",
            fontsize=10, color=INK, pad=10,
        )
        ax.set_aspect("equal")
        ax.set_xlim(-extent, extent)
        ax.set_ylim(-extent, extent)
        ax.set_xlabel("x (AU)")
        ax.grid(alpha=0.5, zorder=0)
        ax.set_axisbelow(True)

    axes[0].set_ylabel("y (AU)")
    fig.suptitle(
        f"{duration:.0f} years of a 1 AU circular orbit — exact answer is a single closed circle",
        fontsize=11, color=INK, x=0.01, ha="left", y=1.04,
    )

    _save(fig, "orbit_shapes.png")
    return fig


def equal_cost_comparison(force_eval_budget=200_000):
    """Compare integrators at equal force evaluations rather than equal dt.

    RK4 spends 4 force evaluations per step and Verlet 2, so comparing at equal
    dt quietly hands RK4 twice the compute. Here each method gets the same
    budget and picks its own dt.

    At 200 orbits RK4 still wins outright (~7e-6 against Verlet's ~1e-4). The
    difference is in the shape: Verlet's envelope is flat while RK4's climbs,
    so the ranking inverts somewhere in the thousands of orbits.
    """
    import matplotlib.pyplot as plt

    scenario = eccentric()
    duration = scenario.period * 200

    fig, ax = plt.subplots(figsize=(8, 4.6))

    labels = []
    for name in ORDER:
        steps = force_eval_budget // FORCE_EVALS_PER_STEP[name]
        dt = duration / steps
        result = run(scenario, name, dt=dt, duration=duration)

        drift = result.energy_drift
        nonzero = drift > 0
        times = result.times[nonzero] / scenario.period
        values = drift[nonzero]

        times, values = _envelope(times, values)

        ax.plot(times, values, color=COLORS[name], linewidth=2,
                solid_capstyle="round", zorder=3)
        labels.append((times[-1], _band_top(values),
                       f"{LABELS[name]}  ·  dt={dt:.2e}", COLORS[name]))

    ax.set_yscale("log")
    ax.set_xlabel("orbits completed")
    ax.set_ylabel("relative energy error  |ΔE / E₀|")
    ax.set_title(
        f"Equal-cost comparison: {force_eval_budget:,} force evaluations each "
        f"(eccentric orbit, e≈0.36)",
        fontsize=11, pad=14, loc="left",
    )
    ax.grid(axis="y", zorder=0)
    ax.set_axisbelow(True)
    ax.margins(x=0.02)
    ax.set_xlim(right=(duration / scenario.period) * 1.25)

    _place_labels(ax, labels)

    fig.text(0.01, -0.04,
             "Same compute budget, each method picking its own dt (RK4 spends 4 force "
             "evaluations per step, Verlet 2). RK4 is more accurate here — but its error "
             "climbs while Verlet's stays flat.",
             fontsize=8.5, color=INK_SECONDARY)

    _save(fig, "equal_cost.png")
    return fig


def _save(fig, filename):
    FIGURES.mkdir(exist_ok=True)
    path = FIGURES / filename
    fig.savefig(path)
    print(f"  wrote {path.relative_to(FIGURES.parent)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--show", action="store_true", help="display the figures too")
    parser.add_argument(
        "--only",
        choices=["energy", "convergence", "orbits", "cost"],
        help="generate just one figure",
    )
    args = parser.parse_args()

    if not args.show:
        matplotlib.use("Agg")
    _style()

    figures = {
        "energy": energy_drift,
        "convergence": convergence_order,
        "orbits": orbit_shapes,
        "cost": equal_cost_comparison,
    }
    selected = [args.only] if args.only else list(figures)

    for key in selected:
        print(f"generating {key}...")
        figures[key]()

    if args.show:
        import matplotlib.pyplot as plt
        plt.show()


if __name__ == "__main__":
    main()
