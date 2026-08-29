#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "core/System.h"

namespace core {

// Gravitational acceleration on every body from every other body, O(n^2).
//
//   a_i = sum_{j != i} G * m_j * (r_j - r_i) / |r_j - r_i|^3
//
// `out` is resized if needed; it's an out-parameter so the caller can reuse
// one allocation across many steps.
//
// The positions/masses overload is for multi-stage integrators like RK4, which
// evaluate at intermediate positions that aren't the system's current ones.
void computeAccelerations(const std::vector<glm::dvec3>& positions,
                          const std::vector<double>& masses,
                          std::vector<glm::dvec3>& out);

void computeAccelerations(const System& system, std::vector<glm::dvec3>& out);

}  // namespace core
