#include "core/Scenarios.h"

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

}  // namespace scenarios
}  // namespace core
