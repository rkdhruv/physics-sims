#include "core/Diagnostics.h"

#include <glm/geometric.hpp>

namespace core {

double totalEnergy(const System& system, const ForceModel& forces) {
  const std::size_t n = system.size();

  double ke = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    ke += 0.5 * system.masses[i] * glm::dot(system.velocities[i], system.velocities[i]);
  }

  return ke + forces.potentialEnergy(system.positions, system.masses);
}

// L = sum_i m_i * (r_i x v_i)
glm::dvec3 angularMomentum(const System& system) {
  const std::size_t n = system.size();

  glm::dvec3 L(0.0);
  for (std::size_t i = 0; i < n; ++i) {
    L += system.masses[i] * glm::cross(system.positions[i], system.velocities[i]);
  }

  return L;
}

}  // namespace core
