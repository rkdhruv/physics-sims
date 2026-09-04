# benchmarks

```bash
cmake --build build -j8
./build/nbody_bench > benchmarks/results.csv
./venv/bin/python -m validation.plot_benchmark      # writes figures/nbody_scaling.png
```

`results.csv` is timing data, so it means nothing without the machine it came
from. The committed run is from an **Apple M2 Pro, Apple clang 15, Release
(-O3), single-threaded**. Absolute numbers will differ elsewhere; the exponents
and the crossover point are the parts that transfer.

## What the committed run measured

| | |
|---|---|
| Direct summation | n^1.99 — textbook O(n²) |
| Barnes-Hut (θ=0.5) | n^1.61 over the same range, n^1.27 over the largest four sizes |
| Crossover | ~512 bodies |
| Speedup at 16,384 bodies | 4.3×, for 0.3% force error |

## Why Barnes-Hut isn't n^1.1 here

Two follow-up measurements, both worth knowing before reading the exponent as a
disappointment.

**Tree construction is under 1% of the runtime** (8.6 ms of 1218 ms at 65k
bodies). Traversal dominates entirely.

**Nodes visited per body grows as ~n^0.35, not log n:**

| n | visits/body | log₂(n) | ns per visit |
|---|---|---|---|
| 1,024 | 484 | 10 | 6.4 |
| 4,096 | 938 | 12 | 8.2 |
| 16,384 | 1,499 | 14 | 8.7 |
| 65,536 | 2,075 | 16 | 8.9 |

Time per visit is nearly flat, so this isn't a cache effect — the traversal is
genuinely doing more work per body as n grows.

The reason is the distribution. Barnes-Hut's O(n log n) bound assumes roughly
uniform density, and a Plummer sphere is strongly centrally concentrated: a body
in the dense core has many neighbours close enough to fail the opening
criterion, so the walk descends deep through a large share of the tree. The
asymptotic bound is a claim about the input as much as the algorithm.

θ is the lever if speed matters more than accuracy: θ=1.0 opens far fewer cells,
at ~4.5% force error instead of ~0.8%.
