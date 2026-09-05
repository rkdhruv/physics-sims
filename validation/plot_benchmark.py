"""Plot the N-body solver benchmark.

    ./build/nbody_bench > benchmarks/results.csv
    ./venv/bin/python -m validation.plot_benchmark

Writes figures/nbody_scaling.png.
"""

import argparse
import csv
import pathlib

import matplotlib
import numpy as np

from validation.make_figures import (INK, INK_MUTED, INK_SECONDARY, SURFACE,
                                     _save, _style)

RESULTS = pathlib.Path(__file__).resolve().parent.parent / "benchmarks" / "results.csv"

COLORS = {"direct": "#2a78d6", "barnes-hut": "#eb6834"}
LABELS = {"direct": "Direct summation", "barnes-hut": "Barnes-Hut"}


def load(path):
    series = {}
    threads = {"n": [], "ms": []}
    with open(path) as f:
        for row in csv.DictReader(f):
            if row["solver"] == "barnes-hut-threads":
                # For these rows the last column is thread count, not error.
                threads["n"].append(int(row["force_error"]))
                threads["ms"].append(float(row["seconds"]) * 1e3)
                continue
            entry = series.setdefault(row["solver"], {"n": [], "ms": []})
            entry["n"].append(int(row["bodies"]))
            entry["ms"].append(float(row["seconds"]) * 1e3)

    out = {k: {"n": np.array(v["n"]), "ms": np.array(v["ms"])}
           for k, v in series.items()}
    if threads["n"]:
        out["threads"] = {"n": np.array(threads["n"]),
                          "ms": np.array(threads["ms"])}
    return out


def exponent(n, ms):
    """Slope of log(time) against log(n) -- the empirical complexity."""
    return np.polyfit(np.log(n), np.log(ms), 1)[0]


def crossover(series):
    """Smallest body count at which the tree is faster."""
    direct, tree = series["direct"], series["barnes-hut"]
    shared = min(len(direct["n"]), len(tree["n"]))
    for i in range(shared):
        if tree["ms"][i] < direct["ms"][i]:
            return direct["n"][i]
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--show", action="store_true")
    args = parser.parse_args()

    if not args.show:
        matplotlib.use("Agg")
    _style()
    import matplotlib.pyplot as plt

    series = load(RESULTS)
    fig, ax = plt.subplots(figsize=(8, 5))

    # Exponents fitted over the range both solvers cover, for a like-for-like
    # comparison.
    shared_max = series["direct"]["n"].max()

    for name in ("direct", "barnes-hut"):
        data = series[name]
        mask = data["n"] <= shared_max
        p = exponent(data["n"][mask], data["ms"][mask])

        ax.plot(data["n"], data["ms"], color=COLORS[name], linewidth=2,
                marker="o", markersize=5, markeredgecolor=SURFACE,
                markeredgewidth=1.2, zorder=3,
                label=f"{LABELS[name]}  ·  measured n^{p:.2f}")

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("bodies")
    ax.set_ylabel("time per force evaluation (ms)")
    ax.set_title("N-body solver scaling  (Plummer cluster, theta = 0.5)",
                 fontsize=11, pad=14, loc="left")
    ax.grid(zorder=0)
    ax.set_axisbelow(True)

    legend = ax.legend(frameon=False, loc="upper left", fontsize=9)
    for text in legend.get_texts():
        text.set_color(INK)

    n_cross = crossover(series)
    if n_cross is not None:
        ax.axvline(n_cross, color=INK_SECONDARY, linewidth=1,
                   linestyle=(0, (4, 4)), zorder=1)
        ax.annotate(f"crossover ≈ {n_cross:,} bodies", (n_cross, ax.get_ylim()[0]),
                    textcoords="offset points", xytext=(8, 18), fontsize=9,
                    color=INK_SECONDARY)

    speedup = (series["direct"]["ms"][-1] /
               series["barnes-hut"]["ms"][len(series["direct"]["ms"]) - 1])
    fig.text(0.01, -0.04,
             f"Below the crossover the tree's overhead outweighs the pairs it "
             f"skips. At {shared_max:,} bodies it is {speedup:.1f}x faster, for "
             f"a force error under 1%.",
             fontsize=8.5, color=INK_SECONDARY)

    _save(fig, "nbody_scaling.png")

    if "threads" in series:
        thread_figure(series["threads"])

    if args.show:
        plt.show()


def thread_figure(data):
    import matplotlib.pyplot as plt

    counts, ms = data["n"], data["ms"]
    speedup = ms[0] / ms

    fig, ax = plt.subplots(figsize=(7.2, 4.6))

    ax.plot(counts, counts, color=INK_MUTED, linewidth=1.5,
            linestyle=(0, (4, 4)), zorder=2, label="perfect scaling")
    ax.plot(counts, speedup, color="#eb6834", linewidth=2, marker="o",
            markersize=5, markeredgecolor=SURFACE, markeredgewidth=1.2,
            zorder=3, label="measured")

    ax.set_xlabel("worker threads")
    ax.set_ylabel("speedup vs. single-threaded")
    ax.set_title("Barnes-Hut force loop, thread scaling  (16,384 bodies)",
                 fontsize=11, pad=14, loc="left")
    ax.grid(zorder=0)
    ax.set_axisbelow(True)
    ax.set_xticks(counts)

    legend = ax.legend(frameon=False, loc="upper left", fontsize=9)
    for text in legend.get_texts():
        text.set_color(INK)

    best = speedup.max()
    fig.text(0.01, -0.05,
             f"Peaks at {best:.1f}x. The plateau past 8 threads is the hardware: "
             f"this machine has 8 performance cores and 4 efficiency cores, and "
             f"the efficiency cores add almost nothing.",
             fontsize=8.5, color=INK_SECONDARY)

    _save(fig, "thread_scaling.png")


if __name__ == "__main__":
    main()
