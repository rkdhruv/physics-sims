#pragma once

#include <glm/vec3.hpp>

#include "core/System.h"

namespace core {

// Total mechanical energy: kinetic + potential.
//   KE = sum_i (1/2) m_i |v_i|^2
//   PE = -sum_{i<j} G m_i m_j / |r_i - r_j|
double totalEnergy(const System& system);

// Total angular momentum about the origin.
//
// A vector here, unlike the 2D Python version where the cross product collapses
// to its z-component. The direction is the orbital plane's normal, so a tilt in
// it indicates an out-of-plane perturbation.
glm::dvec3 angularMomentum(const System& system);

}  // namespace core
