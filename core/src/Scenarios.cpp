#include "core/Scenarios.h"

#include <cmath>

#include "core/Elements.h"
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

}  // namespace scenarios
}  // namespace core
