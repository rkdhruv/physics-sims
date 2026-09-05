// Test suite for the C++ engine core.
//
//   cmake --build build && ./build/core_tests
//
// No test framework, so the build has no dependency beyond CMake and a
// compiler. A test that throws std::logic_error is reported as SKIP rather
// than FAIL, so one missing function doesn't mask the results behind it;
// skips still count against the exit code.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "core/BarnesHut.h"
#include "core/Parallel.h"
#include "core/Diagnostics.h"
#include "core/Elements.h"
#include "core/ForceModel.h"
#include "core/Integrator.h"
#include "core/Scenarios.h"
#include "core/System.h"
#include "core/Units.h"

namespace {

// The heliocentric model the week-1/2 scenarios and the Python oracle use.
const core::NBodyGravity kSolar(core::solar::kG);

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
bool integrate(core::System& system, double dt, double duration, core::Method method,
               const core::ForceModel& forces = kSolar) {
  const int steps = static_cast<int>(std::llround(duration / dt));
  try {
    for (int i = 0; i < steps; ++i) core::step(system, dt, method, forces);
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
  kSolar.accelerations(s, a);

  // Gravity attracts: a body at +x accelerates toward -x.
  CHECK(a[1].x < 0.0);

  // The massless test particle exerts no force on the Sun.
  CHECK(glm::length(a[0]) == 0.0);

  // Inverse square: double the separation, quarter the acceleration.
  core::System far = s;
  far.positions[1] = {2.0, 0.0, 0.0};
  std::vector<glm::dvec3> a_far;
  kSolar.accelerations(far, a_far);
  CHECK_MSG(nearly(glm::length(a[1]), 4.0 * glm::length(a_far[1]), 1e-12),
            describe(glm::length(a[1]), 4.0 * glm::length(a_far[1])));

  // Circular orbit condition: |a| = GM/r^2 = 4*pi^2 at r=1.
  CHECK_MSG(nearly(glm::length(a[1]), core::solar::kG, 1e-12),
            describe(glm::length(a[1]), core::solar::kG));
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
    CHECK_MSG(nearly(core::totalEnergy(s, kSolar), 0.5 - core::solar::kG, 1e-12),
              describe(core::totalEnergy(s, kSolar), 0.5 - core::solar::kG));
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
  const double want = -core::solar::kG * (1.0 + 1.0 + 1.0 / std::sqrt(2.0));

  try {
    CHECK_MSG(nearly(core::totalEnergy(three, kSolar), want, 1e-12),
              describe(core::totalEnergy(three, kSolar), want));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // A bound orbit has negative total energy.
    core::System earth = core::scenarios::sunEarth();
    CHECK(core::totalEnergy(earth, kSolar) < 0.0);

    // And it must match the Python oracle exactly.
    const double python_value = -5.9276844032942678e-05;
    CHECK_MSG(nearly(core::totalEnergy(earth, kSolar), python_value, 1e-12),
              describe(core::totalEnergy(earth, kSolar), python_value));
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
      core::step(s, dt, core::Method::Verlet, kSolar);
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

// ---------------------------------------------------------------------------
// Orbital elements
// ---------------------------------------------------------------------------

void testElements() {
  std::printf("orbital elements\n");

  try {
    // Round trip: a known circular orbit should give back what built it.
    const double altitude = 500.0;
    const double inclination = 45.0;
    core::System s = core::scenarios::circularOrbit(altitude, inclination);
    const core::OrbitalElements el =
        core::toElements(s.positions[0], s.velocities[0]);

    const double expected_a = core::earth::kRadius + altitude;
    CHECK_MSG(nearly(el.semi_major_axis, expected_a, 1e-9),
              describe(el.semi_major_axis, expected_a));
    CHECK_MSG(el.eccentricity < 1e-12, describe(el.eccentricity, 0.0));
    CHECK_MSG(nearly(core::degrees(el.inclination), inclination, 1e-9),
              describe(core::degrees(el.inclination), inclination));

    // Molniya is built at the critical inclination with a 12-hour period.
    core::System m = core::scenarios::molniya();
    const core::OrbitalElements mel =
        core::toElements(m.positions[0], m.velocities[0]);
    CHECK_MSG(nearly(core::degrees(mel.inclination), 63.435, 1e-6),
              describe(core::degrees(mel.inclination), 63.435));
    CHECK_MSG(nearly(core::orbitalPeriod(mel.semi_major_axis), 12.0 * 3600.0, 1e-6),
              describe(core::orbitalPeriod(mel.semi_major_axis), 12.0 * 3600.0));
    // Started at perigee, so true anomaly is 0 (or 2pi).
    const double anomaly = std::min(mel.true_anomaly,
                                    2.0 * core::kPi - mel.true_anomaly);
    CHECK_MSG(anomaly < 1e-9, describe(anomaly, 0.0));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // An orbit propagated with no perturbation keeps its elements: the shape
    // and orientation are constants of the unperturbed two-body problem, so
    // only the true anomaly may move.
    const core::EarthOrbit two_body({/*j2=*/false});
    core::System s = core::scenarios::iss();
    const core::OrbitalElements before =
        core::toElements(s.positions[0], s.velocities[0]);

    if (integrate(s, 1.0, 20.0 * 5400.0, core::Method::RK4, two_body)) {
      const core::OrbitalElements after =
          core::toElements(s.positions[0], s.velocities[0]);

      CHECK_MSG(nearly(after.semi_major_axis, before.semi_major_axis, 1e-9),
                describe(after.semi_major_axis, before.semi_major_axis));
      CHECK_MSG(nearly(after.inclination, before.inclination, 1e-9),
                describe(core::degrees(after.inclination),
                         core::degrees(before.inclination)));
      CHECK_MSG(std::abs(after.raan - before.raan) < 1e-9,
                describe(core::degrees(after.raan), core::degrees(before.raan)));
    }
  } catch (const std::logic_error& e) {
    skip(e.what());
  }
}

// ---------------------------------------------------------------------------
// J2 perturbation
//
// These don't check the acceleration formula directly. They propagate an orbit
// with J2 switched on, measure how fast the node actually moves, and compare
// against the analytic secular rate. Two independent routes to the same number
// agreeing is what makes both credible.
// ---------------------------------------------------------------------------

void testJ2() {
  std::printf("J2 perturbation\n");

  const core::EarthOrbit with_j2({/*j2=*/true});

  try {
    // The perturbation is small: at LEO, J2 contributes ~1e-3 of the central
    // term. Much larger and it would be a different orbit, not a perturbation.
    core::System s = core::scenarios::iss();
    std::vector<glm::dvec3> a;
    with_j2.accelerations(s, a);

    const core::EarthOrbit two_body({/*j2=*/false});
    std::vector<glm::dvec3> a0;
    two_body.accelerations(s, a0);

    const double ratio = glm::length(a[0] - a0[0]) / glm::length(a0[0]);
    CHECK_MSG(ratio > 1e-4 && ratio < 1e-2, describe(ratio, 1e-3));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // An equatorial orbit sits in the bulge's plane of symmetry, so J2 pulls
    // straight inward with no out-of-plane component.
    core::System equatorial = core::scenarios::circularOrbit(500.0, 0.0);
    const glm::dvec3 aj2 = with_j2.j2Acceleration(equatorial.positions[0]);
    CHECK_MSG(std::abs(aj2.z) < 1e-15, describe(aj2.z, 0.0));

    // Over the pole the term is purely along z, and points *outward*: the
    // bulge's mass sits farther from a polar satellite than an equivalent
    // sphere's would, so gravity there is weaker than the point-mass value.
    // The correction is inward over the equator and outward over the poles.
    const glm::dvec3 polar = with_j2.j2Acceleration({0.0, 0.0, 7000.0});
    CHECK(std::abs(polar.x) < 1e-15 && std::abs(polar.y) < 1e-15);
    CHECK(polar.z > 0.0);
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // The model must honour its Config rather than reaching for the global
    // Earth constants. Nothing else here exercises this, because every other
    // test uses the defaults -- where the two are indistinguishable.
    const glm::dvec3 r{7000.0, 0.0, 3000.0};

    core::EarthOrbit::Config doubled;
    doubled.j2_coefficient = core::earth::kJ2 * 2.0;
    const double base = glm::length(with_j2.j2Acceleration(r));
    const double twice = glm::length(core::EarthOrbit(doubled).j2Acceleration(r));
    CHECK_MSG(nearly(twice, 2.0 * base, 1e-12), describe(twice / base, 2.0));

    // Same for mu: the J2 term scales linearly with it.
    core::EarthOrbit::Config half_mu;
    half_mu.mu = core::earth::kMu * 0.5;
    const double halved = glm::length(core::EarthOrbit(half_mu).j2Acceleration(r));
    CHECK_MSG(nearly(halved, 0.5 * base, 1e-12), describe(halved / base, 0.5));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // The headline test: propagate a sun-synchronous orbit for a day and check
    // the node moved at the analytically predicted rate.
    core::System s = core::scenarios::sunSynchronous();
    const core::OrbitalElements start =
        core::toElements(s.positions[0], s.velocities[0]);
    const double predicted = core::nodalPrecessionRate(start);

    const double duration = 86400.0;
    if (integrate(s, 1.0, duration, core::Method::RK4, with_j2)) {
      const core::OrbitalElements end =
          core::toElements(s.positions[0], s.velocities[0]);

      double measured = (end.raan - start.raan) / duration;
      // Unwrap in case RAAN crossed the 0/2pi branch cut.
      if (std::abs(measured) > core::kPi / duration) {
        measured = (end.raan - start.raan - 2.0 * core::kPi) / duration;
      }

      char detail[192];
      std::snprintf(detail, sizeof(detail),
                    "nodal rate: measured %.6e rad/s, predicted %.6e rad/s "
                    "(%.3f%% apart)",
                    measured, predicted,
                    100.0 * std::abs(measured - predicted) / std::abs(predicted));
      std::printf("  nodal precession over 24h: %.4f deg/day measured, "
                  "%.4f predicted (%.2f%% apart)\n",
                  core::degrees(measured) * 86400.0,
                  core::degrees(predicted) * 86400.0,
                  100.0 * std::abs(measured - predicted) / std::abs(predicted));
      // 1% -- first-order secular theory ignores short-period oscillations
      // that don't average out exactly over a whole number of days.
      CHECK_MSG(std::abs(measured - predicted) < 0.01 * std::abs(predicted), detail);
    }
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // Sun-synchronous means the node precesses one full turn per year, which
    // is what keeps the local solar time of each pass fixed. At 700 km the
    // inclination that achieves it is a little over 98 degrees -- retrograde,
    // which is why those launches fly slightly west of south.
    const double a = core::earth::kRadius + 700.0;
    const double i = core::degrees(core::sunSynchronousInclination(a));
    CHECK_MSG(i > 98.0 && i < 98.5, describe(i, 98.2));

    core::OrbitalElements el;
    el.semi_major_axis = a;
    el.eccentricity = 0.0;
    el.inclination = core::radians(i);

    const double rate = core::nodalPrecessionRate(el);
    const double required = 2.0 * core::kPi / core::earth::kSiderealYear;
    CHECK_MSG(nearly(rate, required, 1e-6), describe(rate, required));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // At the critical inclination the argument of perigee stops drifting,
    // because 5cos^2(i) - 1 vanishes at i = 63.43 degrees.
    core::OrbitalElements el;
    el.semi_major_axis = 26600.0;
    el.eccentricity = 0.74;
    el.inclination = core::radians(63.435);

    CHECK(std::abs(core::apsidalPrecessionRate(el)) < 1e-11);

    // Away from it, perigee moves. Below the critical angle it advances.
    el.inclination = core::radians(30.0);
    CHECK(core::apsidalPrecessionRate(el) > 0.0);
    el.inclination = core::radians(80.0);
    CHECK(core::apsidalPrecessionRate(el) < 0.0);
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // A prograde orbit's node regresses (moves west); a retrograde one's
    // advances. The sign is what makes sun-synchronous orbits need i > 90.
    core::OrbitalElements prograde;
    prograde.semi_major_axis = 7000.0;
    prograde.inclination = core::radians(51.6);
    CHECK(core::nodalPrecessionRate(prograde) < 0.0);

    core::OrbitalElements retrograde = prograde;
    retrograde.inclination = core::radians(120.0);
    CHECK(core::nodalPrecessionRate(retrograde) > 0.0);
  } catch (const std::logic_error& e) {
    skip(e.what());
  }
}

// ---------------------------------------------------------------------------
// Ground track
// ---------------------------------------------------------------------------

void testGroundTrack() {
  std::printf("ground track\n");

  try {
    // On the +x axis at t=0 the frames are aligned, so longitude is 0 and the
    // point is on the equator.
    const core::GroundPoint p =
        core::toGroundPoint({core::earth::kRadius + 400.0, 0.0, 0.0}, 0.0);
    CHECK_MSG(std::abs(p.latitude) < 1e-9, describe(p.latitude, 0.0));
    CHECK_MSG(std::abs(p.longitude) < 1e-9, describe(p.longitude, 0.0));
    CHECK_MSG(nearly(p.altitude, 400.0, 1e-9), describe(p.altitude, 400.0));

    // Straight over the north pole.
    const core::GroundPoint pole = core::toGroundPoint({0.0, 0.0, 7000.0}, 0.0);
    CHECK_MSG(nearly(pole.latitude, 90.0, 1e-9), describe(pole.latitude, 90.0));

    // Latitude can never exceed the inclination.
    core::System s = core::scenarios::iss();
    const core::EarthOrbit model;
    double worst = 0.0;
    double out_of_range = 0.0;
    const double dt = 10.0;
    for (int step = 0; step < 1000; ++step) {
      core::step(s, dt, core::Method::RK4, model);
      const core::GroundPoint g = core::toGroundPoint(s.positions[0], step * dt);
      worst = std::max(worst, std::abs(g.latitude));
      if (g.longitude < -180.0 || g.longitude > 180.0) {
        out_of_range = g.longitude;
      }
    }
    // One check for the whole sweep rather than a thousand.
    CHECK_MSG(out_of_range == 0.0, describe(out_of_range, 0.0));
    CHECK_MSG(worst < 52.0, describe(worst, 51.6));
    CHECK_MSG(worst > 50.0, describe(worst, 51.6));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }
}

// Barnes-Hut, checked against the exact O(n^2) solver: at theta = 0 the tree
// never approximates, so the two must agree to rounding.
void testBarnesHut() {
  std::printf("Barnes-Hut\n");

  const double softening = core::scenarios::kClusterSoftening;
  const core::NBodyGravity exact(1.0, softening);

  core::System s = core::scenarios::cluster(512);
  std::vector<glm::dvec3> reference;
  exact.accelerations(s, reference);

  // Force error normalised by the system's RMS acceleration. Dividing each
  // particle's error by its own acceleration instead would be dominated by
  // core particles, whose forces nearly cancel.
  auto forceError = [&](double theta) {
    const core::BarnesHutGravity tree(1.0, theta, softening);
    std::vector<glm::dvec3> approx;
    tree.accelerations(s, approx);

    double error_sq = 0.0, reference_sq = 0.0;
    for (std::size_t i = 0; i < reference.size(); ++i) {
      const glm::dvec3 d = approx[i] - reference[i];
      error_sq += glm::dot(d, d);
      reference_sq += glm::dot(reference[i], reference[i]);
    }
    return reference_sq > 0.0 ? std::sqrt(error_sq / reference_sq) : 0.0;
  };

  try {
    // Exact pairwise summation by a different route. Not bit-identical: the
    // sum runs in a different order and addition isn't associative.
    const double exact_error = forceError(0.0);
    char detail[160];
    std::snprintf(detail, sizeof(detail),
                  "theta=0 must reproduce direct summation: force error %.3e",
                  exact_error);
    CHECK_MSG(exact_error < 1e-12, detail);
    std::printf("  theta=0 vs direct summation: %.3e force error\n", exact_error);
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // Accuracy degrades monotonically as the opening angle grows, and 0.5 --
    // the conventional default -- should still be within about a percent.
    const double e02 = forceError(0.2);
    const double e05 = forceError(0.5);
    const double e10 = forceError(1.0);

    std::printf("  error vs theta: 0.2 -> %.2e, 0.5 -> %.2e, 1.0 -> %.2e\n",
                e02, e05, e10);

    CHECK(e02 < e05);
    CHECK(e05 < e10);
    CHECK_MSG(e05 < 1e-2, describe(e05, 1e-2));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // The root must hold every body, which catches bodies dropped during
    // insertion -- otherwise visible only as a slightly wrong force.
    const core::BarnesHutGravity tree(1.0, 0.5, softening);
    std::vector<glm::dvec3> ignored;
    tree.accelerations(s, ignored);

    const core::OctreeNode& root = tree.tree().nodes().at(0);

    double total_mass = 0.0;
    glm::dvec3 com(0.0);
    for (std::size_t i = 0; i < s.size(); ++i) {
      total_mass += s.masses[i];
      com += s.masses[i] * s.positions[i];
    }
    com /= total_mass;

    CHECK_MSG(nearly(root.mass, total_mass, 1e-12), describe(root.mass, total_mass));
    CHECK_MSG(glm::length(root.com - com) < 1e-12,
              describe(glm::length(root.com - com), 0.0));

    // O(n) nodes; far more means cells subdividing without separating anything.
    const std::size_t nodes = tree.tree().nodeCount();
    char detail[128];
    std::snprintf(detail, sizeof(detail), "%zu nodes for %zu bodies, depth %d",
                  nodes, s.size(), tree.tree().depth());
    CHECK_MSG(nodes >= s.size() / 8 && nodes < 20 * s.size(), detail);
    std::printf("  %zu bodies -> %zu nodes, depth %d\n", s.size(), nodes,
                tree.tree().depth());
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // Node count against n. Counted rather than timed, so it stays
    // deterministic -- wall-clock assertions belong in the benchmark.
    const core::BarnesHutGravity tree(1.0, 0.5, softening);
    std::vector<double> log_n, log_nodes;

    for (std::size_t n : {256u, 512u, 1024u, 2048u}) {
      core::System c = core::scenarios::cluster(n);
      std::vector<glm::dvec3> out;
      tree.accelerations(c, out);
      log_n.push_back(std::log(static_cast<double>(n)));
      log_nodes.push_back(std::log(static_cast<double>(tree.tree().nodeCount())));
    }

    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    const std::size_t k = log_n.size();
    for (std::size_t i = 0; i < k; ++i) {
      sx += log_n[i];
      sy += log_nodes[i];
      sxx += log_n[i] * log_n[i];
      sxy += log_n[i] * log_nodes[i];
    }
    const double slope = (k * sxy - sx * sy) / (k * sxx - sx * sx);

    char detail[128];
    std::snprintf(detail, sizeof(detail),
                  "node count should scale ~linearly with n, measured n^%.2f",
                  slope);
    CHECK_MSG(std::abs(slope - 1.0) < 0.2, detail);
    std::printf("  node count scales as n^%.2f\n", slope);
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  try {
    // A virialised cluster starts with 2T + U = 0.
    core::System c = core::scenarios::cluster(256);
    const core::BarnesHutGravity tree(1.0, 0.5, softening);

    double kinetic = 0.0;
    for (std::size_t i = 0; i < c.size(); ++i) {
      kinetic += 0.5 * c.masses[i] * glm::dot(c.velocities[i], c.velocities[i]);
    }
    const double potential = tree.potentialEnergy(c.positions, c.masses);

    CHECK_MSG(std::abs(2.0 * kinetic + potential) < 1e-9,
              describe(2.0 * kinetic + potential, 0.0));
  } catch (const std::logic_error& e) {
    skip(e.what());
  }
}

// Threading the force loop must not change the answer at all. Each body writes
// its own slot and the arithmetic per body is unchanged, so this is bitwise
// equality, not a tolerance -- anything less would mean a real data race.
void testParallel() {
  std::printf("parallel force loop\n");

  const double softening = core::scenarios::kClusterSoftening;
  core::System s = core::scenarios::cluster(4096);

  auto bitwiseIdentical = [](const std::vector<glm::dvec3>& a,
                             const std::vector<glm::dvec3>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
      if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z) return false;
    }
    return true;
  };

  try {
    std::vector<glm::dvec3> serial, parallel;

    core::BarnesHutGravity(1.0, 0.5, softening, 1).accelerations(s, serial);
    for (unsigned t : {2u, 4u, 8u, 0u}) {  // 0 = all cores
      core::BarnesHutGravity(1.0, 0.5, softening, t).accelerations(s, parallel);
      char detail[96];
      std::snprintf(detail, sizeof(detail),
                    "Barnes-Hut: %u threads differs from serial", t);
      CHECK_MSG(bitwiseIdentical(serial, parallel), detail);
    }

    core::NBodyGravity(1.0, softening, 1).accelerations(s, serial);
    for (unsigned t : {2u, 8u, 0u}) {
      core::NBodyGravity(1.0, softening, t).accelerations(s, parallel);
      char detail[96];
      std::snprintf(detail, sizeof(detail),
                    "direct: %u threads differs from serial", t);
      CHECK_MSG(bitwiseIdentical(serial, parallel), detail);
    }

    std::printf("  bit-identical across 1..%u threads\n", core::hardwareThreads());
  } catch (const std::logic_error& e) {
    skip(e.what());
  }

  // Every index must be written exactly once: chunks have to tile [0, count)
  // with no gap and no overlap.
  std::vector<int> visits(10000, 0);
  core::parallelFor(visits.size(), 0, [&](std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) visits[i] = 1;
  });
  CHECK(std::count(visits.begin(), visits.end(), 1) ==
        static_cast<long>(visits.size()));

  // Degenerate inputs shouldn't spawn anything or run off the end.
  core::parallelFor(0, 0, [](std::size_t, std::size_t) {
    std::printf("  FAIL: empty range ran work\n");
  });
  std::size_t single = 0;
  core::parallelFor(1, 8, [&](std::size_t b, std::size_t e) { single += e - b; });
  CHECK(single == 1);
}

}  // namespace

int main() {
  std::printf("core_tests\n\n");

  testForces();
  testDiagnostics();
  testConvergenceOrder();
  testOrbitStability();
  testAgainstPythonReference();
  testElements();
  testJ2();
  testGroundTrack();
  testBarnesHut();
  testParallel();

  std::printf("\n%d checks, %d failed, %d skipped\n", g_checks, g_failures, g_skips);
  if (g_skips > 0) {
    std::printf("skipped tests mean unwritten functions -- not a pass\n");
  }
  return (g_failures == 0 && g_skips == 0) ? 0 : 1;
}
