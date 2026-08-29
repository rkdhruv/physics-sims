// Test suite for the C++ engine core.
//
//   cmake --build build && ./build/core_tests
//
// No test framework, so the build has no dependency beyond CMake and a
// compiler. A test that throws std::logic_error is reported as SKIP rather
// than FAIL, so one missing function doesn't mask the results behind it;
// skips still count against the exit code.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "core/Diagnostics.h"
#include "core/Forces.h"
#include "core/Integrator.h"
#include "core/Scenarios.h"
#include "core/System.h"
#include "core/Units.h"

namespace {

int g_failures = 0;
int g_checks = 0;
int g_skips = 0;

void skip(const std::string& reason) {
  ++g_skips;
  std::printf("  SKIP (%s)\n", reason.c_str());
}

void report(bool ok, const char* expression, const char* file, int line,
            const std::string& detail) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("  FAIL %s:%d\n       %s\n", file, line, expression);
  if (!detail.empty()) std::printf("       %s\n", detail.c_str());
}

#define CHECK(expr) report((expr), #expr, __FILE__, __LINE__, "")
#define CHECK_MSG(expr, detail) report((expr), #expr, __FILE__, __LINE__, (detail))

bool nearly(double a, double b, double tolerance) {
  return std::abs(a - b) <= tolerance * std::max(1.0, std::abs(b));
}

std::string describe(double got, double want) {
  char buffer[160];
  std::snprintf(buffer, sizeof(buffer), "got %.17g, want %.17g", got, want);
  return buffer;
}

// Runs a system forward and returns the final state. Catches the
// not-implemented exceptions so one missing function doesn't abort the run.
bool integrate(core::System& system, double dt, double duration, core::Method method) {
  const int steps = static_cast<int>(std::llround(duration / dt));
  try {
    for (int i = 0; i < steps; ++i) core::step(system, dt, method);
  } catch (const std::logic_error& e) {
    skip(e.what());
    return false;
  }
  return true;
}

glm::dvec3 analyticCircularPosition(double t) {
  const double omega = 2.0 * core::kPi;
  return {std::cos(omega * t), std::sin(omega * t), 0.0};
}

// --------------------------------------------------------------------------

void testForces() {
  std::printf("forces\n");

  core::System s = core::scenarios::testParticle();
  std::vector<glm::dvec3> a;
  core::computeAccelerations(s, a);

  // Gravity attracts: a body at +x accelerates toward -x.
  CHECK(a[1].x < 0.0);

  // The massless test particle exerts no force on the Sun.
  CHECK(glm::length(a[0]) == 0.0);

  // Inverse square: double the separation, quarter the acceleration.
  core::System far = s;
  far.positions[1] = {2.0, 0.0, 0.0};
  std::vector<glm::dvec3> a_far;
  core::computeAccelerations(far, a_far);
  CHECK_MSG(nearly(glm::length(a[1]), 4.0 * glm::length(a_far[1]), 1e-12),
            describe(glm::length(a[1]), 4.0 * glm::length(a_far[1])));

  // Circular orbit condition: |a| = GM/r^2 = 4*pi^2 at r=1.
  CHECK_MSG(nearly(glm::length(a[1]), core::kG, 1e-12),
            describe(glm::length(a[1]), core::kG));
}

void testDiagnostics() {
  std::printf("diagnostics\n");

  // Two unit masses 1 AU apart, one moving at 1 AU/yr.
  //   KE = 0.5,  PE = -G  (one pair, counted once)
  core::System s;
  s.resize(2);
  s.positions = {{0, 0, 0}, {1, 0, 0}};
  s.velocities = {{0, 0, 0}, {0, 1, 0}};
  s.masses = {1.0, 1.0};

  try {
    CHECK_MSG(nearly(core::totalEnergy(s), 0.5 - core::kG, 1e-12),
              describe(core::totalEnergy(s), 0.5 - core::kG));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  // Three equal masses -- catches the i<j double-counting mistake, which a
  // two-body case can hide.
  core::System three;
  three.resize(3);
  three.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  three.velocities = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
  three.masses = {1.0, 1.0, 1.0};
  const double want = -core::kG * (1.0 + 1.0 + 1.0 / std::sqrt(2.0));

  try {
    CHECK_MSG(nearly(core::totalEnergy(three), want, 1e-12),
              describe(core::totalEnergy(three), want));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // A bound orbit has negative total energy.
    core::System earth = core::scenarios::sunEarth();
    CHECK(core::totalEnergy(earth) < 0.0);

    // And it must match the Python oracle exactly.
    const double python_value = -5.9276844032942678e-05;
    CHECK_MSG(nearly(core::totalEnergy(earth), python_value, 1e-12),
              describe(core::totalEnergy(earth), python_value));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    const glm::dvec3 l = core::angularMomentum(s);
    // r x v for a body at +x moving toward +y points along +z.
    CHECK(l.z > 0.0);
    CHECK(std::abs(l.x) < 1e-15 && std::abs(l.y) < 1e-15);
    CHECK_MSG(nearly(l.z, 1.0, 1e-12), describe(l.z, 1.0));

    core::System earth = core::scenarios::sunEarth();
    const double python_value = 1.8868405477460296e-05;
    CHECK_MSG(nearly(core::angularMomentum(earth).z, python_value, 1e-12),
              describe(core::angularMomentum(earth).z, python_value));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }
}

// Measure the empirical order of accuracy: fit the slope of log(error) against
// log(dt). Same method as the Python suite, same expected answers.
void testConvergenceOrder() {
  std::printf("convergence order\n");

  struct Case {
    core::Method method;
    double expected_order;
    std::vector<double> timesteps;
  };

  // Per-method dt ranges, chosen to sit in the asymptotic regime where error
  // actually scales as dt^p. Euler at dt=1e-2 has an error larger than the
  // orbit itself and measures ~0.83 instead of 1.
  const std::vector<Case> cases = {
      {core::Method::Euler, 1.0, {1e-3, 5e-4, 2.5e-4}},
      {core::Method::Verlet, 2.0, {1e-2, 5e-3, 2.5e-3}},
      {core::Method::RK4, 4.0, {1e-2, 5e-3, 2.5e-3}},
  };

  for (const Case& c : cases) {
    std::vector<double> log_dt, log_err;
    bool ok = true;

    for (double dt : c.timesteps) {
      core::System s = core::scenarios::testParticle();
      if (!integrate(s, dt, 1.0, c.method)) {
        ok = false;
        break;
      }
      const double error = glm::length(s.positions[1] - analyticCircularPosition(1.0));
      log_dt.push_back(std::log(dt));
      log_err.push_back(std::log(error));
    }
    if (!ok) continue;

    // Least-squares slope.
    const std::size_t n = log_dt.size();
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (std::size_t i = 0; i < n; ++i) {
      sx += log_dt[i];
      sy += log_err[i];
      sxx += log_dt[i] * log_dt[i];
      sxy += log_dt[i] * log_err[i];
    }
    const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);

    char detail[128];
    std::snprintf(detail, sizeof(detail), "%s: measured order %.2f, expected %.1f",
                  core::methodName(c.method), slope, c.expected_order);
    CHECK_MSG(std::abs(slope - c.expected_order) < 0.25, detail);
  }
}

void testOrbitStability() {
  std::printf("orbit stability\n");

  // Euler must visibly fail, or the diagnostics aren't detecting anything.
  core::System euler = core::scenarios::testParticle();
  if (integrate(euler, 1e-3, 10.0, core::Method::Euler)) {
    CHECK(glm::length(euler.positions[1]) > 1.05);
  }

  // Verlet and RK4 must hold the radius to 0.1% over 10 orbits.
  for (core::Method method : {core::Method::Verlet, core::Method::RK4}) {
    core::System s = core::scenarios::testParticle();
    if (!integrate(s, 1e-3, 10.0, method)) continue;
    const double radius = glm::length(s.positions[1]);
    CHECK_MSG(std::abs(radius - 1.0) < 1e-3, describe(radius, 1.0));
  }
}

// Replay a trajectory generated by the Python harness and require agreement.
void testAgainstPythonReference() {
  std::printf("cross-validation against Python\n");

  std::ifstream file(REFERENCE_DATA);
  if (!file) {
    skip("no reference data -- run ./venv/bin/python -m validation.export_reference");
    return;
  }

  std::vector<double> times;
  std::vector<glm::dvec3> earth;

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#' || line[0] == 't') continue;

    std::stringstream row(line);
    std::string cell;
    std::vector<double> values;
    while (std::getline(row, cell, ',')) values.push_back(std::stod(cell));

    // t, x0,y0,z0, x1,y1,z1
    if (values.size() < 7) continue;
    times.push_back(values[0]);
    earth.push_back({values[4], values[5], values[6]});
  }

  if (times.size() < 2) {
    skip("reference data unreadable");
    return;
  }

  const double dt = 1.0 / 365.25;
  core::System s = core::scenarios::sunEarth();

  double worst = 0.0;
  std::size_t worst_step = 0;
  try {
    for (std::size_t step = 1; step < times.size(); ++step) {
      core::step(s, dt, core::Method::Verlet);
      const double error = glm::length(s.positions[1] - earth[step]);
      if (error > worst) {
        worst = error;
        worst_step = step;
      }
    }
  } catch (const std::logic_error& e) {
    skip(e.what());
    return;
  }

  char detail[192];
  std::snprintf(detail, sizeof(detail),
                "worst divergence %.3e AU at step %zu of %zu -- two independent "
                "implementations should agree to ~1e-12",
                worst, worst_step, times.size());
  std::printf("  max divergence from Python over %zu steps: %.3e AU\n",
              times.size() - 1, worst);
  CHECK_MSG(worst < 1e-12, detail);
}

}  // namespace

int main() {
  std::printf("core_tests\n\n");

  testForces();
  testDiagnostics();
  testConvergenceOrder();
  testOrbitStability();
  testAgainstPythonReference();

  std::printf("\n%d checks, %d failed, %d skipped\n", g_checks, g_failures, g_skips);
  if (g_skips > 0) {
    std::printf("skipped tests mean unwritten functions -- not a pass\n");
  }
  return (g_failures == 0 && g_skips == 0) ? 0 : 1;
}
