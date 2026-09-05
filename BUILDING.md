# Building

## Dependencies

```bash
brew install cmake glfw glm
xcode-select --install     # Command Line Tools; full Xcode.app is NOT needed
```

The GL loader in `vendor/glad/` is already generated and committed, so nothing
else is required. See `vendor/README.md` if you ever need to regenerate it.

## Build and run

```bash
cmake -S . -B build
cmake --build build -j8

./build/orbital        # integrator comparison, heliocentric
./build/satellite      # Earth orbits with J2 and a ground track
./build/cluster        # N-body cluster, solver switchable at runtime
./build/core_tests     # the engine test suite
./build/nbody_bench    # solver timing (see benchmarks/)
```

`cmake -S . -B build` only needs re-running when `CMakeLists.txt` changes or a
source file is added. Day to day, `cmake --build build` is enough.

## Building without graphics

`core/` links no graphics libraries, so the engine and its test suite build on a
machine with no GPU, no display and no GLFW — only a compiler and glm:

```bash
cmake -S . -B build -DBUILD_SCENES=OFF
cmake --build build -j8 && ./build/core_tests
```

This is what CI runs. It's also the fastest way to iterate on the physics, since
it skips the renderer entirely.

## Controls

Shared by all three scenes:

| | |
|---|---|
| left drag | orbit the camera |
| right drag | pan |
| scroll | zoom |
| `space` | pause |
| `R` | reload shaders from disk |
| `P` | screenshot (writes a `.ppm` next to the binary) |
| `esc` | quit |

Scene-specific:

| | |
|---|---|
| `1` `2` `3` in `orbital` | Euler / Verlet / RK4 |
| `1` `2` `3` in `satellite` | ISS / sun-synchronous / Molniya |
| `J` in `satellite` | toggle the J2 perturbation |
| `[` `]` `C` in `orbital`, `satellite` | slow down / speed up / clear trails |
| `1` `2` in `cluster` | direct summation / Barnes-Hut |
| `[` `]` in `cluster` | halve / double the body count |
| `-` `=` in `cluster` | opening angle θ down / up |
| `,` `.` in `cluster` | point size |
| `C` in `cluster` | restart |

`R` reloads shaders from `render/shaders/` at runtime, so a `.frag` can be
edited and seen without a rebuild.

`J` is the demonstration worth running: the title bar reports `dRAAN`, the
drift of the orbital plane since the run started. With J2 off it stays pinned
at zero. With it on, ISS drifts about −4.97 °/day and the sun-synchronous orbit
about +0.99 °/day — one full turn per year, which is the entire point of that
orbit.

Note that the large westward shift between successive ground-track passes is
Earth's rotation (~22.5° per 90-minute orbit), not J2. The perturbation is the
slow additional drift on top, and the `dRAAN` readout is where it's legible.

In `cluster`, `1` and `2` are the comparison worth running: the title bar reports
milliseconds per step, so switching solvers shows the speedup directly. At 4096
bodies direct summation is ~118 ms/step against the tree's ~31 ms. Larger body
counts cost frame rate — 8192 runs at ~13 fps with θ=0.7, and raising θ trades
accuracy for speed.

## Screenshots

`P` writes the framebuffer to a binary PPM — no image library needed in the
renderer. Convert with:

```bash
./venv/bin/python tools/ppm2png.py satellite-1234.ppm docs/molniya.png --trim
```

`--trim` crops the uniform background from the edges, which orbit captures tend
to have a lot of, and the output is downscaled to 1600 px wide. A Retina
framebuffer is ~3400 px across — several megabytes of PNG for no visible gain at
the width GitHub renders at.

Capture a handful of orbits rather than a long run. The scenes keep six orbits
of history by default; leaving one running for minutes fills the ground-track
panel with an unreadable mesh. `C` clears the trails if a run has gone too far.

## Sanitizer build

Worth reaching for the moment anything segfaults, misbehaves inexplicably, or
produces numbers that are subtly wrong:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g -fno-omit-frame-pointer" \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j8
./build-asan/core_tests
```

AddressSanitizer catches out-of-bounds accesses, use-after-free, and leaks;
UndefinedBehaviorSanitizer catches signed overflow, bad shifts, and null
dereferences. Both cost roughly 2x runtime, which is why they're a separate
build directory rather than always on.

ThreadSanitizer is a separate build again — it can't be combined with ASan —
and is what checks the parallel force loop for races:

```bash
cmake -S . -B build-tsan -DBUILD_SCENES=OFF -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan -j8 && ./build-tsan/core_tests
```

The payoff over a plain debugger is that you get the offending line directly.
A raw segfault gives you `exit code 139` and nothing else — worse, because
stdout is block-buffered, a crash usually swallows all the output printed
before it, so it looks like the program did nothing at all.

## Python harness

```bash
./venv/bin/pytest                              # 34 tests
./venv/bin/python -m validation.make_figures   # writes figures/
./venv/bin/python -m validation.export_reference   # regenerate the C++ oracle data
```

## Troubleshooting

**`glfw3Config.cmake` not found** — Homebrew on Apple Silicon installs to
`/opt/homebrew`, which isn't on CMake's default search path. CMakeLists.txt
already adds it; if you're on an Intel Mac it's `/usr/local` instead.

**Window opens black** — the trails need a few frames of simulation before
there's anything to draw. If it stays black, check that the camera hasn't been
zoomed inside the orbit (`scroll` out, or restart).

**Shader compile errors on startup** — the error log includes line numbers into
the `.vert`/`.frag` file. Note the GLSL version must stay `#version 410 core`;
macOS rejects anything higher.

**`Undefined symbols for architecture arm64` mentioning `glad`** — the build is
picking up a stale `build/` from a different compiler. Delete it and
re-configure.
