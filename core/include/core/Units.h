#pragma once

namespace core {

constexpr double kPi = 3.14159265358979323846;

constexpr double degrees(double radians) { return radians * 180.0 / kPi; }
constexpr double radians(double degrees) { return degrees * kPi / 180.0; }

// Heliocentric: AU, solar masses, years. Matches the Python harness so the two
// can be compared value-for-value. G = 4*pi^2 follows from Kepler's third law.
//
// Scaled rather than SI: SI would put positions near 1e11 and masses near 1e30
// in the same expression, spending precision on exponent range.
namespace solar {
constexpr double kG = 4.0 * kPi * kPi;  // AU^3 / (M_sun * yr^2)
}

// Geocentric: kilometres and seconds, the convention in astrodynamics. A
// satellite's own mass never enters the equations, so what matters is the
// gravitational parameter mu = GM rather than G and M separately -- mu is
// measured directly from spacecraft tracking and is known far more precisely
// than either factor.
namespace earth {
constexpr double kMu = 398600.4418;        // km^3/s^2
constexpr double kRadius = 6378.137;       // km, equatorial (WGS-84)
constexpr double kJ2 = 1.08262668e-3;      // oblateness coefficient
constexpr double kRotationRate = 7.2921159e-5;  // rad/s, sidereal

// Sidereal year in seconds -- the rate a sun-synchronous orbit's node must
// precess at to keep a fixed angle to the Sun.
constexpr double kSiderealYear = 31558149.76;
}  // namespace earth

}  // namespace core
