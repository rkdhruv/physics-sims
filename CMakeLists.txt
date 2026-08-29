cmake_minimum_required(VERSION 3.20)
project(physics_sims LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # for clangd / IDE autocomplete

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Homebrew lives outside CMake's default search paths on Apple Silicon.
if(APPLE)
  list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /opt/homebrew/opt/glfw /opt/homebrew/opt/glm)
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# glad -- OpenGL function loader, generated into vendor/ (GL 4.1 core, which
# is the highest version macOS supports).
# ---------------------------------------------------------------------------
add_library(glad STATIC vendor/glad/src/gl.c)
target_include_directories(glad PUBLIC vendor/glad/include)

# ---------------------------------------------------------------------------
# core -- the simulation engine. Links no graphics libraries, so the physics
# stays testable without opening a window.
# ---------------------------------------------------------------------------
add_library(core STATIC
  core/src/Forces.cpp
  core/src/Integrator.cpp
  core/src/Diagnostics.cpp
  core/src/Scenarios.cpp
)
target_include_directories(core PUBLIC core/include)
target_link_libraries(core PUBLIC glm::glm)

# ---------------------------------------------------------------------------
# render -- windowing, shaders, camera. Knows OpenGL, knows no physics.
# ---------------------------------------------------------------------------
add_library(render STATIC
  render/src/Window.cpp
  render/src/Shader.cpp
  render/src/Camera.cpp
)
target_include_directories(render PUBLIC render/include)
target_link_libraries(render PUBLIC glad glfw glm::glm)

if(APPLE)
  target_link_libraries(render PUBLIC "-framework OpenGL" "-framework Cocoa"
                                      "-framework IOKit" "-framework CoreVideo")
endif()

# Shaders are read from disk at runtime, so R reloads them without a rebuild.
target_compile_definitions(render PUBLIC
  SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/render/shaders")

# ---------------------------------------------------------------------------
# scenes
# ---------------------------------------------------------------------------
add_executable(orbital scenes/orbital/main.cpp)
target_link_libraries(orbital PRIVATE core render)

# ---------------------------------------------------------------------------
# tests -- plain assert-based executable, no test framework dependency.
# ---------------------------------------------------------------------------
add_executable(core_tests tests/cpp/test_core.cpp)
target_link_libraries(core_tests PRIVATE core)
target_compile_definitions(core_tests PRIVATE
  REFERENCE_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference_trajectory.csv")

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
