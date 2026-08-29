#include "core/Forces.h"

#include <glm/geometric.hpp>

#include "core/Units.h"

namespace core {

void computeAccelerations(const std::vector<glm::dvec3>& positions,
                          const std::vector<double>& masses,
                          std::vector<glm::dvec3>& out) {
  const std::size_t n = masses.size();
  out.assign(n, glm::dvec3(0.0));

  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      if (i == j) continue;

      const glm::dvec3 d = positions[j] - positions[i];
      const double r2 = glm::dot(d, d);  // dot, not length: avoids a sqrt
      const double inv_r3 = 1.0 / (r2 * std::sqrt(r2));

      out[i] += kG * masses[j] * d * inv_r3;
    }
  }
}

void computeAccelerations(const System& system, std::vector<glm::dvec3>& out) {
  computeAccelerations(system.positions, system.masses, out);
}

}  // namespace core
