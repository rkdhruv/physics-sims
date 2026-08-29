#include "core/Diagnostics.h"

#include <stdexcept>

#include <glm/geometric.hpp>

#include "core/Units.h"

namespace core {

double totalEnergy(const System& system) {
  const std::size_t n = system.size();

  double ke = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    ke += 0.5 * system.masses[i] * glm::dot(system.velocities[i], system.velocities[i]);
  }

  double pe = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i + 1; j < n; ++j) {  // unique pairs only
      const glm::dvec3 r = system.positions[i] - system.positions[j];
      pe -= kG * system.masses[i] * system.masses[j] / glm::length(r);
    }
  }

  return ke + pe;
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
