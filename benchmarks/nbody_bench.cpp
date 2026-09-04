// Times direct summation against the Barnes-Hut tree across body counts.
//
//   ./build/nbody_bench > benchmarks/results.csv
//
// Separate from core_tests on purpose: wall-clock assertions fail on a loaded
// CI runner for reasons unrelated to the code.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "core/BarnesHut.h"
#include "core/ForceModel.h"
#include "core/Scenarios.h"
#include "core/System.h"

namespace {

// Direct summation stops earlier: at 32k bodies one evaluation is a billion
// interactions.
const std::vector<std::size_t> kDirectSizes = {
    128, 256, 512, 1024, 2048, 4096, 8192, 16384};
const std::vector<std::size_t> kTreeSizes = {
    128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};

// Repeat until this much time has elapsed, so small runs aren't dominated by
// clock resolution.
constexpr double kMinSeconds = 0.15;

struct Timing {
  double seconds_per_eval = 0.0;
  int repetitions = 0;
};

Timing time(const core::ForceModel& model, const core::System& system) {
  std::vector<glm::dvec3> out;

  // One untimed evaluation to warm the cache and pay the tree's first
  // allocation.
  model.accelerations(system, out);

  const auto start = std::chrono::steady_clock::now();
  int reps = 0;
  double elapsed = 0.0;
  do {
    model.accelerations(system, out);
    ++reps;
    elapsed = std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - start).count();
  } while (elapsed < kMinSeconds);

  return {elapsed / reps, reps};
}

// Normalised by the system's RMS acceleration, matching the test suite.
double forceError(const std::vector<glm::dvec3>& approx,
                  const std::vector<glm::dvec3>& exact) {
  double error_sq = 0.0, exact_sq = 0.0;
  for (std::size_t i = 0; i < exact.size(); ++i) {
    const glm::dvec3 d = approx[i] - exact[i];
    error_sq += glm::dot(d, d);
    exact_sq += glm::dot(exact[i], exact[i]);
  }
  return exact_sq > 0.0 ? std::sqrt(error_sq / exact_sq) : 0.0;
}

}  // namespace

int main() {
  const double softening = core::scenarios::kClusterSoftening;
  const core::NBodyGravity direct(1.0, softening);
  const core::BarnesHutGravity tree(1.0, 0.5, softening);

  std::printf("solver,bodies,seconds,repetitions,force_error\n");
  std::fflush(stdout);

  // Direct summation, and the accelerations the tree gets scored against.
  std::vector<std::vector<glm::dvec3>> exact_by_size;
  for (std::size_t n : kDirectSizes) {
    const core::System system = core::scenarios::cluster(n);
    const Timing t = time(direct, system);

    std::vector<glm::dvec3> exact;
    direct.accelerations(system, exact);
    exact_by_size.push_back(std::move(exact));

    std::printf("direct,%zu,%.9g,%d,0\n", n, t.seconds_per_eval, t.repetitions);
    std::fflush(stdout);
    std::fprintf(stderr, "  direct   n=%-6zu %8.3f ms\n", n,
                 t.seconds_per_eval * 1e3);
  }

  for (std::size_t i = 0; i < kTreeSizes.size(); ++i) {
    const std::size_t n = kTreeSizes[i];
    const core::System system = core::scenarios::cluster(n);
    const Timing t = time(tree, system);

    // Blank past the sizes direct summation covers: a 0 would read as exact.
    std::string error_field;
    if (i < exact_by_size.size()) {
      std::vector<glm::dvec3> approx;
      tree.accelerations(system, approx);
      char buffer[32];
      std::snprintf(buffer, sizeof(buffer), "%.9g",
                    forceError(approx, exact_by_size[i]));
      error_field = buffer;
    }

    std::printf("barnes-hut,%zu,%.9g,%d,%s\n", n, t.seconds_per_eval,
                t.repetitions, error_field.c_str());
    std::fflush(stdout);
    std::fprintf(stderr, "  tree     n=%-6zu %8.3f ms   force error %s\n", n,
                 t.seconds_per_eval * 1e3,
                 error_field.empty() ? "n/a" : error_field.c_str());
  }

  return 0;
}
