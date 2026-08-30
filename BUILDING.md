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

./build/orbital        # the scene
./build/core_tests     # the engine test suite
```

`cmake -S . -B build` only needs re-running when `CMakeLists.txt` changes or a
source file is added. Day to day, `cmake --build build` is enough.

## Controls

| | |
|---|---|
| left drag | orbit the camera |
| right drag | pan |
| scroll | zoom |
| `space` | pause |
| `1` `2` `3` | Euler / Verlet / RK4 |
| `[` `]` | slow down / speed up |
| `R` | reload shaders from disk |
| `C` | clear trails |
| `esc` | quit |

`R` is the one worth knowing about: shaders are read from `render/shaders/` at
runtime, so you can edit a `.frag`, press R, and see the change without a
rebuild. That turns a ~20-second edit loop into an instant one, which matters a
lot while you're learning GLSL.

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
