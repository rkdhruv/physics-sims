#include "core/ForceModel.h"

#include <cmath>
#include <stdexcept>

#include <glm/geometric.hpp>

#include "core/Parallel.h"

namespace core {

// ---------------------------------------------------------------------------
// NBodyGravity
// ---------------------------------------------------------------------------

NBodyGravity::NBodyGravity(double G, double softening, unsigned threads)
    : G_(G), softening_(softening), threads_(threads) {}

void NBodyGravity::accelerations(const std::vector<glm::dvec3>& positions,
                                 const std::vector<double>& masses,
                                 std::vector<glm::dvec3>& out) const {
  const std::size_t n = masses.size();
  out.assign(n, glm::dvec3(0.0));

  const double eps2 = softening_ * softening_;

  parallelFor(n, threads_, [&](std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        if (i == j) continue;

        const glm::dvec3 d = positions[j] - positions[i];
        const double r2 = glm::dot(d, d) + eps2;  // dot, not length: avoids a sqrt
        const double inv_r3 = 1.0 / (r2 * std::sqrt(r2));

        out[i] += G_ * masses[j] * d * inv_r3;
      }
    }
  });
}

double NBodyGravity::potentialEnergy(const std::vector<glm::dvec3>& positions,
                                     const std::vector<double>& masses) const {
  const std::size_t n = masses.size();
  const double eps2 = softening_ * softening_;

  double pe = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i + 1; j < n; ++j) {  // unique pairs only
      const glm::dvec3 d = positions[i] - positions[j];
      pe -= G_ * masses[i] * masses[j] / std::sqrt(glm::dot(d, d) + eps2);
    }
  }
  return pe;
}

// ---------------------------------------------------------------------------
// EarthOrbit
// ---------------------------------------------------------------------------

EarthOrbit::EarthOrbit() = default;
EarthOrbit::EarthOrbit(Config config) : config_(config) {}

void EarthOrbit::accelerations(const std::vector<glm::dvec3>& positions,
                               const std::vector<double>& masses,
                               std::vector<glm::dvec3>& out) const {
  (void)masses;  // satellite mass cancels out of its own equation of motion

  const std::size_t n = positions.size();
  out.assign(n, glm::dvec3(0.0));

  const double mu = config_.mu;

  for (std::size_t i = 0; i < n; ++i) {
    const glm::dvec3& r = positions[i];
    const double r2 = glm::dot(r, r);
    const double r_mag = std::sqrt(r2);

    // Two-body point mass: a = -mu * r / |r|^3
    out[i] = -mu * r / (r2 * r_mag);

    if (config_.j2) out[i] += j2Acceleration(r);
  }
}

// Acceleration from Earth's equatorial bulge -- Earth is ~21 km wider across
// the equator than pole to pole. By far the largest perturbation on a low
// orbit, and the reason sun-synchronous orbits exist.
//
//     k   = (3/2) * J2 * mu * Re^2 / r^5
//     a_x = -k * x * (1 - 5*z^2/r^2)
//     a_y = -k * y * (1 - 5*z^2/r^2)
//     a_z = -k * z * (3 - 5*z^2/r^2)
//
// The 3 in the z component where x and y have a 1 is the entire effect: it
// makes the correction point inward over the equator and outward over the
// poles. With a 1 there the orbit still looks fine and the node never moves.
glm::dvec3 EarthOrbit::j2Acceleration(const glm::dvec3& r) const {
  const double r2 =  glm::dot(r, r);
  const double rmag = std::sqrt(r2);
  const double r5 = r2 * r2 * rmag;

  const double k = 1.5 * config_.j2_coefficient * config_.mu * config_.radius * config_.radius / r5;
  const double zr2 = 5 * r.z * r.z / r2;

  return glm::dvec3(-k * r.x * (1 - zr2), -k * r.y * (1- zr2), -k * r.z * (3 - zr2));

}

double EarthOrbit::potentialEnergy(const std::vector<glm::dvec3>& positions,
                                   const std::vector<double>& masses) const {
  // Specific potential energy summed over bodies (per unit satellite mass),
  // two-body term only. The J2 correction to the potential is not included --
  // the diagnostics that matter for this model are the orbital elements.
  (void)masses;
  double pe = 0.0;
  for (const glm::dvec3& r : positions) {
    pe -= config_.mu / glm::length(r);
  }
  return pe;
}

}  // namespace core
