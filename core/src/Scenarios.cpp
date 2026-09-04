#include "core/Scenarios.h"

#include <cmath>
#include <random>

#include <glm/geometric.hpp>

#include "core/Elements.h"
#include "core/ForceModel.h"
#include "core/Units.h"

namespace core {
namespace scenarios {

System sunEarth() {
  System s;
  s.resize(2);
  s.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
  s.velocities = {{0.0, 0.0, 0.0}, {0.0, 2.0 * kPi, 0.0}};
  s.masses = {1.0, 3.003e-6};
  return s;
}

System testParticle() {
  System s;
  s.resize(2);
  s.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
  s.velocities = {{0.0, 0.0, 0.0}, {0.0, 2.0 * kPi, 0.0}};
  s.masses = {1.0, 0.0};
  return s;
}

System eccentric() {
  // 80% of circular velocity at 1 AU. vis-viva derivation of a and the period
  // is in validation/scenarios.py.
  const double v0 = 0.8 * (2.0 * kPi);

  System s;
  s.resize(2);
  s.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
  s.velocities = {{0.0, 0.0, 0.0}, {0.0, v0, 0.0}};
  s.masses = {1.0, 3.003e-6};
  return s;
}

// ---------------------------------------------------------------------------
// Geocentric scenarios, km and seconds.
// ---------------------------------------------------------------------------

System circularOrbit(double altitude_km, double inclination_deg) {
  const double r = earth::kRadius + altitude_km;
  const double v = std::sqrt(earth::kMu / r);  // circular speed
  const double i = radians(inclination_deg);

  // Start at the ascending node on the +x axis, so the velocity carries the
  // full inclination: tilting it here rather than tilting the position keeps
  // RAAN at zero and makes the node drift easy to read off later.
  System s;
  s.resize(1);
  s.positions = {{r, 0.0, 0.0}};
  s.velocities = {{0.0, v * std::cos(i), v * std::sin(i)}};
  s.masses = {1.0};
  return s;
}

System iss() { return circularOrbit(420.0, 51.6); }

System sunSynchronous() {
  const double altitude = 700.0;
  const double a = earth::kRadius + altitude;
  const double i = degrees(sunSynchronousInclination(a));
  return circularOrbit(altitude, i);
}

System molniya() {
  // Semi-major axis for a 12-hour period, from Kepler's third law. With
  // perigee at 600 km altitude that gives a ~ 26562 km and e ~ 0.737.
  const double period = 12.0 * 3600.0;
  const double a = std::cbrt(earth::kMu * (period / (2.0 * kPi)) * (period / (2.0 * kPi)));
  const double r_perigee = earth::kRadius + 600.0;

  // Starting at perigee: vis-viva gives the speed, v^2 = mu(2/r - 1/a).
  const double v = std::sqrt(earth::kMu * (2.0 / r_perigee - 1.0 / a));
  const double i = radians(63.435);

  System s;
  s.resize(1);
  s.positions = {{r_perigee, 0.0, 0.0}};
  s.velocities = {{0.0, v * std::cos(i), v * std::sin(i)}};
  s.masses = {1.0};
  return s;
}

System cluster(std::size_t count, unsigned seed, double softening) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<double> uniform(0.0, 1.0);

  System s;
  s.resize(count);

  for (std::size_t i = 0; i < count; ++i) {
    s.masses[i] = 1.0 / static_cast<double>(count);

    // Plummer radius sampler, from inverting the enclosed-mass profile. u is
    // nudged off zero, which would place a body at infinity.
    const double u = std::max(uniform(rng), 1e-9);
    const double r = 1.0 / std::sqrt(std::pow(u, -2.0 / 3.0) - 1.0);

    // Uniform over the sphere: cos(theta) uniform, not theta, or the points
    // bunch at the poles.
    const double cos_theta = 2.0 * uniform(rng) - 1.0;
    const double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);
    const double phi = 2.0 * kPi * uniform(rng);

    s.positions[i] = r * glm::dvec3(sin_theta * std::cos(phi),
                                    sin_theta * std::sin(phi),
                                    cos_theta);
  }

  // Random velocity directions, magnitudes filled in by the virial scaling.
  std::normal_distribution<double> gaussian(0.0, 1.0);
  for (std::size_t i = 0; i < count; ++i) {
    s.velocities[i] = glm::dvec3(gaussian(rng), gaussian(rng), gaussian(rng));
  }

  // Remove net drift so the cluster stays put. Masses are equal, so this is
  // just the mean.
  glm::dvec3 drift(0.0);
  for (const glm::dvec3& v : s.velocities) drift += v;
  drift /= static_cast<double>(count);
  for (glm::dvec3& v : s.velocities) v -= drift;

  // Virial equilibrium: 2T + U = 0, so T = -U/2. U comes from the exact solver
  // rather than the Plummer analytic value, so it matches the softening in use.
  const NBodyGravity exact(1.0, softening);
  const double potential = exact.potentialEnergy(s.positions, s.masses);

  double kinetic = 0.0;
  for (std::size_t i = 0; i < count; ++i) {
    kinetic += 0.5 * s.masses[i] * glm::dot(s.velocities[i], s.velocities[i]);
  }

  if (kinetic > 0.0) {
    const double scale = std::sqrt(-0.5 * potential / kinetic);
    for (glm::dvec3& v : s.velocities) v *= scale;
  }

  return s;
}

}  // namespace scenarios
}  // namespace core
