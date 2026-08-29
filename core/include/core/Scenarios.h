#pragma once

#include "core/System.h"

namespace core {

// The same initial conditions as validation/scenarios.py, lifted into 3D with
// z = 0, so the cross-validation test can compare the two implementations.
namespace scenarios {

// Sun + Earth, circular orbit at 1 AU. Period 1 year.
System sunEarth();

// Massless test particle around a fixed Sun, with an exact analytic solution.
// Used by the convergence tests. Not usable for energy or angular momentum:
// both are mass-weighted sums and come out identically zero.
System testParticle();

// Eccentric orbit, e ~ 0.36, a ~ 0.735 AU, period ~ 0.630 yr.
System eccentric();

}  // namespace scenarios
}  // namespace core
