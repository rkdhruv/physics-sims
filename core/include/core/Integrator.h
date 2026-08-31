#pragma once

#include "core/ForceModel.h"
#include "core/System.h"

namespace core {

enum class Method {
  Euler,   // 1st order, 1 force eval/step  -- the control group
  Verlet,  // 2nd order, 2 force evals/step -- symplectic
  RK4,     // 4th order, 4 force evals/step
};

// Advance the system by one timestep, in place. (The Python version returns
// new arrays instead; here copying the whole state per step would dominate.)
//
// The integrator knows nothing about the physics -- swapping `forces` between
// heliocentric N-body and Earth-plus-J2 changes the simulation without
// touching a line here.
void step(System& system, double dt, Method method, const ForceModel& forces);

// Human-readable name, for logs and window titles.
const char* methodName(Method method);

// Force evaluations per step -- the honest cost metric when comparing methods.
int forceEvalsPerStep(Method method);

}  // namespace core
