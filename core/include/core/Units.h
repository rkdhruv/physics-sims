#pragma once

namespace core {

// Units: AU, solar masses, years -- matching the Python harness so the two can
// be compared value-for-value. G = 4*pi^2 follows from Kepler's third law.
//
// Scaled rather than SI: SI would put positions near 1e11 and masses near 1e30
// in the same expression, spending precision on exponent range.
constexpr double kPi = 3.14159265358979323846;
constexpr double kG = 4.0 * kPi * kPi;

}  // namespace core
