#pragma once

#include <glm/vec3.hpp>

#include "core/Units.h"

namespace core {

// Classical Keplerian elements.
//
// A position and velocity vector describe where a satellite is right now; the
// elements describe the shape and orientation of the orbit it's on. For an
// unperturbed two-body orbit five of the six are constant and only the true
// anomaly advances, which is what makes them the right coordinates for
// measuring a perturbation: J2 shows up as a steady drift in raan and
// arg_periapsis that would otherwise be invisible under the orbital motion.
struct OrbitalElements {
  double semi_major_axis = 0.0;  // km
  double eccentricity = 0.0;
  double inclination = 0.0;      // rad, from the equatorial plane
  double raan = 0.0;             // rad, right ascension of the ascending node
  double arg_periapsis = 0.0;    // rad, periapsis angle from the node
  double true_anomaly = 0.0;     // rad, satellite angle from periapsis

  double periapsis() const { return semi_major_axis * (1.0 - eccentricity); }
  double apoapsis() const { return semi_major_axis * (1.0 + eccentricity); }
  double semi_latus_rectum() const {
    return semi_major_axis * (1.0 - eccentricity * eccentricity);
  }
};

// Convert a state vector to classical elements.
OrbitalElements toElements(const glm::dvec3& position,
                           const glm::dvec3& velocity,
                           double mu = earth::kMu);

// Mean motion, rad/s.
double meanMotion(double semi_major_axis, double mu = earth::kMu);

// Orbital period, seconds.
double orbitalPeriod(double semi_major_axis, double mu = earth::kMu);

// Analytic first-order J2 secular rates -- what the numerical propagation gets
// checked against.
//
// The node regresses and the periapsis rotates at rates that depend only on
// the orbit's size, shape and tilt. Both fall out of averaging the J2
// disturbing potential over one revolution.

// dRAAN/dt, rad/s. Negative for a prograde orbit (the node moves west).
double nodalPrecessionRate(const OrbitalElements& elements,
                           double mu = earth::kMu,
                           double radius = earth::kRadius,
                           double j2 = earth::kJ2);

// d(arg_periapsis)/dt, rad/s. Zero at the critical inclination, 63.4 degrees.
double apsidalPrecessionRate(const OrbitalElements& elements,
                             double mu = earth::kMu,
                             double radius = earth::kRadius,
                             double j2 = earth::kJ2);

// The inclination whose nodal precession matches Earth's orbit around the Sun,
// so the orbital plane holds a fixed angle to the Sun and the satellite crosses
// the equator at the same local solar time every pass. Returns radians;
// slightly retrograde, a little over 90 degrees.
double sunSynchronousInclination(double semi_major_axis,
                                 double eccentricity = 0.0,
                                 double mu = earth::kMu,
                                 double radius = earth::kRadius,
                                 double j2 = earth::kJ2);

// Sub-satellite point: geodetic latitude and longitude in degrees.
//
// `seconds` is time since the epoch at which the inertial frame and the
// rotating Earth frame were aligned -- without subtracting Earth's rotation
// the longitude would be meaningless.
struct GroundPoint {
  double latitude = 0.0;   // degrees, -90..90
  double longitude = 0.0;  // degrees, -180..180
  double altitude = 0.0;   // km above the reference sphere
};

GroundPoint toGroundPoint(const glm::dvec3& position, double seconds,
                          double rotation_rate = earth::kRotationRate,
                          double radius = earth::kRadius);

}  // namespace core
