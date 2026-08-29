# vendor/

Third-party code, committed rather than fetched at build time so that a fresh
clone builds with nothing but CMake and a compiler.

## glad/

OpenGL function loader, generated — not hand-written, and not upstream source.

```
pip install glad2
glad --api="gl:core=4.1" --out-path=vendor/glad c
```

GL 4.1 core because that is the highest version macOS supports; Apple
deprecated OpenGL in 2018 at 4.1 and never shipped past it. On Linux or Windows
this can be regenerated at 4.6 without touching any other code.

## Not vendored

`glfw` and `glm` come from Homebrew (`brew install glfw glm`) and are found by
`find_package` in CMakeLists.txt. They're real libraries with their own release
cadence, so pinning them here would mean maintaining them.
