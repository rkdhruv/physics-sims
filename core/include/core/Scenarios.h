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

// --- Geocentric, km and seconds. Masses are 1.0 so the energy diagnostic
// --- reports specific energy (per unit satellite mass).

// Circular orbit at the given altitude and inclination.
System circularOrbit(double altitude_km, double inclination_deg);

// ISS-like: 420 km circular, 51.6 degrees.
System iss();

// Sun-synchronous at 700 km, at the inclination that makes the node precess
// once per year (a little over 98 degrees).
System sunSynchronous();

// Molniya: highly eccentric, 12-hour period, at the 63.4-degree critical
// inclination where the argument of perigee stops drifting.
System molniya();

// --- N-body units: G = 1, total mass = 1, Plummer scale radius = 1.

// A self-gravitating cluster of `count` equal-mass bodies, with positions
// following a Plummer profile and velocities scaled to virial equilibrium
// (2T + U = 0) so it neither collapses nor disperses.
//
// Deterministic for a given seed.
System cluster(std::size_t count, unsigned seed = 42, double softening = 0.05);

// Must also be passed to the force model, or the energies won't match.
constexpr double kClusterSoftening = 0.05;

}  // namespace scenarios
}  // namespace core
