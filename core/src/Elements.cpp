#include "core/Elements.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/geometric.hpp>

namespace core {
namespace {

// Wrap to [0, 2pi). Element angles are conventionally reported in that range,
// and the drift measurements need a consistent branch to unwrap from.
double wrapTwoPi(double angle) {
  const double two_pi = 2.0 * kPi;
  angle = std::fmod(angle, two_pi);
  return angle < 0.0 ? angle + two_pi : angle;
}

}  // namespace

// State vector -> classical elements, via three intermediate vectors:
//
//   h     = r x v                  angular momentum, normal to the orbit plane
//   n     = z_hat x h              node vector, pointing at the ascending node
//   e_vec = (v x h)/mu - r/|r|     eccentricity vector, pointing at periapsis
//
//   a    = 1 / (2/|r| - |v|^2/mu)  from vis-viva
//   e    = |e_vec|
//   i    = acos(h_z / |h|)
//   raan = acos(n_x / |n|)                            2pi - it if n_y < 0
//   arg_periapsis = acos(dot(n, e_vec) / (|n|*|e|))   2pi - it if e_vec_z < 0
//   true_anomaly  = acos(dot(e_vec, r) / (|e_vec|*|r|))  2pi - it if dot(r,v) < 0
//
// acos only returns 0..pi, so each of the last three needs its sign test to
// recover the full circle. Every acos argument is clamped to [-1, 1] first,
// since a near-circular orbit can produce 1.0000000000000002 and acos of
// anything above 1 is NaN.
//
// raan is undefined for an equatorial orbit and arg_periapsis for a circular
// one; both return 0 in those cases.
OrbitalElements toElements(const glm::dvec3& position,
                           const glm::dvec3& velocity,
                           double mu) {
  constexpr double kTol = 1e-12;

  const double r = glm::length(position);
  const double v2 = glm::dot(velocity, velocity);

  const glm::dvec3 h = glm::cross(position, velocity);
  const glm::dvec3 n = glm::cross(glm::dvec3(0.0, 0.0, 1.0), h);
  const glm::dvec3 e_vec = glm::cross(velocity, h) / earth::kMu - position / r; 
  
  const double h_mag = glm::length(h);
  const double n_mag = glm::length(n);

  OrbitalElements el;

  el.semi_major_axis = 1.0 / (2.0 / r - v2 / earth::kMu);
  el.eccentricity = glm::length(e_vec);
  el.inclination = std::acos(std::clamp(h.z / h_mag, -1.0, 1.0));

  if (n_mag < kTol) {
    el.raan = 0.0;
  } else {
    const double raw = std::acos(std::clamp(n.x / n_mag, -1.0, 1.0));
    el.raan = wrapTwoPi(n.y < 0.0 ? -raw : raw);
  }

  if (n_mag < kTol || el.eccentricity < kTol) {
    el.arg_periapsis = 0.0;
  } else {
    const double raw = std::acos(std::clamp(glm::dot(n, e_vec) / (n_mag * el.eccentricity), -1.0, 1.0));
    el.arg_periapsis = wrapTwoPi(e_vec.z < 0.0 ? -raw : raw);
  }

  if (el.eccentricity < kTol) {
    el.true_anomaly = 0.0;
  } else {
    const double raw = std::acos(std::clamp(glm::dot(e_vec, position) / (el.eccentricity * r), -1.0, 1.0));
    el.true_anomaly = wrapTwoPi(glm::dot(position, velocity) < 0.0 ? -raw : raw);
  }

  return el;
}

double meanMotion(double semi_major_axis, double mu) {
  return std::sqrt(mu / (semi_major_axis * semi_major_axis * semi_major_axis));
}

double orbitalPeriod(double semi_major_axis, double mu) {
  return 2.0 * kPi / meanMotion(semi_major_axis, mu);
}

//     dRAAN/dt = -(3/2) * J2 * (Re/p)^2 * n * cos(i)
//
// p is the semi-latus rectum a(1-e^2), n the mean motion. The minus sign is
// why a prograde orbit's node drifts westward.
double nodalPrecessionRate(const OrbitalElements& elements, double mu,
                           double radius, double j2) {

  const double n = meanMotion(elements.semi_major_axis, mu);
  const double p = elements.semi_latus_rectum();
  const double ratio = radius / p;

  return -1.5 * j2 * ratio * ratio * n * std::cos(elements.inclination);
}

//     d(arg_periapsis)/dt = (3/4) * J2 * (Re/p)^2 * n * (5*cos^2(i) - 1)
//
// The bracket vanishes at cos^2(i) = 1/5, i = 63.43 degrees: the critical
// inclination, where periapsis stops rotating. Molniya orbits use it to keep
// apogee parked over the same hemisphere.
double apsidalPrecessionRate(const OrbitalElements& elements, double mu,
                             double radius, double j2) {

  const double n = meanMotion(elements.semi_major_axis, mu);
  const double p = elements.semi_latus_rectum();
  const double ratio = radius / p;

  const double cos_i = std::cos(elements.inclination);
  return 0.75 * j2 * ratio * ratio * n * (5.0 * cos_i * cos_i - 1.0);
}

double sunSynchronousInclination(double semi_major_axis, double eccentricity,
                                 double mu, double radius, double j2) {
  // Required precession: one full turn per sidereal year, matching Earth's
  // orbit around the Sun.
  const double required = 2.0 * kPi / earth::kSiderealYear;

  const double n = meanMotion(semi_major_axis, mu);
  const double p = semi_major_axis * (1.0 - eccentricity * eccentricity);
  const double factor = 1.5 * j2 * (radius / p) * (radius / p) * n;

  // Inverting dRAAN/dt = -factor * cos(i) for the required positive rate.
  return std::acos(std::clamp(-required / factor, -1.0, 1.0));
}

// Inertial position -> latitude/longitude on the rotating Earth.
//
//   latitude  = asin(z / |r|)
//   longitude = atan2(y, x) - rotation_rate * seconds
//   altitude  = |r| - radius
//
// Subtracting the rotation is what makes a ground track a drifting sine wave
// rather than a closed loop: the orbit is fixed in inertial space while the
// ground turns underneath it. Longitude is wrapped back into range afterwards,
// or it would run off to thousands of degrees over a long propagation.
//
// Spherical latitude, not geodetic -- they differ by at most ~0.2 degrees.
GroundPoint toGroundPoint(const glm::dvec3& position, double seconds,
                          double rotation_rate, double radius) {

  const double r = glm::length(position);

  GroundPoint gp;
  gp.latitude = degrees(std::asin(std::clamp(position.z / r, -1.0, 1.0)));
  
  const double lon_rad = std::atan2(position.y, position.x) - rotation_rate * seconds;
  gp.longitude = degrees(wrapTwoPi(lon_rad));
  if (gp.longitude > 180.0) gp.longitude -= 360.0;

  gp.altitude = r - radius;
  return gp;
}

}  // namespace core
