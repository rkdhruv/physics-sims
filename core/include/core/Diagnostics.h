#pragma once

#include <glm/vec3.hpp>

#include "core/ForceModel.h"
#include "core/System.h"

namespace core {

// Total mechanical energy: kinetic plus the force model's potential.
//
// The potential comes from the model rather than from a fixed formula here,
// since it depends on which physics is in play -- pairwise for N-body, -mu/r
// for a central body.
double totalEnergy(const System& system, const ForceModel& forces);

// Total angular momentum about the origin.
//
// A vector here, unlike the 2D Python version where the cross product collapses
// to its z-component. The direction is the orbital plane's normal, so a tilt in
// it indicates an out-of-plane perturbation.
glm::dvec3 angularMomentum(const System& system);

}  // namespace core
