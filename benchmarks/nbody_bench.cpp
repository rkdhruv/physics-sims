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
#include "core/Parallel.h"
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

// Fixed size for the thread-scaling sweep: large enough that per-body work
// dominates thread startup, small enough to sweep quickly.
constexpr std::size_t kScalingSize = 16384;

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
  // Serial for the complexity sweeps: threading would fold scheduling into the
  // measured exponents. The thread scaling is measured separately below.
  const core::NBodyGravity direct(1.0, softening, 1);
  const core::BarnesHutGravity tree(1.0, 0.5, softening, 1);

  // Last column is force_error for the solver rows and thread count for the
  // barnes-hut-threads rows.
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

  // Thread scaling at a fixed size, so the only variable is worker count.
  std::fprintf(stderr, "\n  scaling at %zu bodies:\n", kScalingSize);
  const core::System scaling_system = core::scenarios::cluster(kScalingSize);
  double serial_seconds = 0.0;

  for (unsigned t : {1u, 2u, 4u, 6u, 8u, 10u, 12u}) {
    if (t > core::hardwareThreads()) break;
    const core::BarnesHutGravity model(1.0, 0.5, softening, t);
    const Timing timing = time(model, scaling_system);
    if (t == 1) serial_seconds = timing.seconds_per_eval;

    std::printf("barnes-hut-threads,%zu,%.9g,%d,%u\n", kScalingSize,
                timing.seconds_per_eval, timing.repetitions, t);
    std::fflush(stdout);
    std::fprintf(stderr, "  %2u thread%s %8.2f ms   %.2fx\n", t,
                 t == 1 ? " " : "s", timing.seconds_per_eval * 1e3,
                 serial_seconds / timing.seconds_per_eval);
  }

  return 0;
}
