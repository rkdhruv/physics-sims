#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "core/System.h"
#include "core/Units.h"

namespace core {

// What acceleration acts on each body.
//
// The integrators only need "given these positions, what are the
// accelerations", so that's the whole interface. Everything else -- which
// units, whether there's a central body, which perturbations are switched on
// -- belongs to the model rather than to the integrator.
class ForceModel {
 public:
  virtual ~ForceModel() = default;

  // `out` is resized as needed. Positions and masses are passed separately
  // rather than as a System because RK4 evaluates at intermediate positions
  // that aren't any System's current state.
  virtual void accelerations(const std::vector<glm::dvec3>& positions,
                             const std::vector<double>& masses,
                             std::vector<glm::dvec3>& out) const = 0;

  void accelerations(const System& system, std::vector<glm::dvec3>& out) const {
    accelerations(system.positions, system.masses, out);
  }

  // Potential energy of the configuration, for the conservation diagnostics.
  virtual double potentialEnergy(const std::vector<glm::dvec3>& positions,
                                 const std::vector<double>& masses) const = 0;
};

// Mutual gravitation between every pair of bodies, O(n^2).
//
//   a_i = sum_{j != i} G * m_j * (r_j - r_i) / (|r_j - r_i|^2 + eps^2)^(3/2)
//
// `softening` (eps) bounds the force at short range: without it a close
// encounter produces an acceleration no fixed timestep can integrate. Defaults
// to zero, which is the classical form.
// `threads` splits the body loop across workers; 0 uses all cores, 1 is
// serial. Each body writes only its own slot, so the arithmetic per body is
// unchanged and results are bit-identical at any thread count.
class NBodyGravity : public ForceModel {
 public:
  explicit NBodyGravity(double G, double softening = 0.0, unsigned threads = 0);

  // Declaring the 3-argument override would otherwise hide the base class's
  // System overload -- C++ name lookup stops at the first scope with a match,
  // so an overload set doesn't merge across a class boundary on its own.
  using ForceModel::accelerations;

  void accelerations(const std::vector<glm::dvec3>& positions,
                     const std::vector<double>& masses,
                     std::vector<glm::dvec3>& out) const override;

  double potentialEnergy(const std::vector<glm::dvec3>& positions,
                         const std::vector<double>& masses) const override;

  double G() const { return G_; }
  double softening() const { return softening_; }

 private:
  double G_;
  double softening_;
  unsigned threads_;
};

// Satellites around a central body fixed at the origin, in km and seconds.
//
// Body masses are ignored: a satellite's mass cancels out of its own equation
// of motion, so every body is a test particle and the central body doesn't
// move. This is the standard formulation for orbit propagation, and it's why
// the model needs mu rather than G and a mass.
class EarthOrbit : public ForceModel {
 public:
  struct Config {
    bool j2 = true;            // oblateness perturbation
    double mu = earth::kMu;
    double radius = earth::kRadius;
    double j2_coefficient = earth::kJ2;
  };

  // Two constructors rather than a defaulted argument: `Config config = {}`
  // would need Config's member initializers while the enclosing class is still
  // incomplete, which the compiler rejects.
  EarthOrbit();
  explicit EarthOrbit(Config config);

  using ForceModel::accelerations;

  void accelerations(const std::vector<glm::dvec3>& positions,
                     const std::vector<double>& masses,
                     std::vector<glm::dvec3>& out) const override;

  double potentialEnergy(const std::vector<glm::dvec3>& positions,
                         const std::vector<double>& masses) const override;

  const Config& config() const { return config_; }

  // Acceleration from Earth's equatorial bulge at position r. Exposed so it
  // can be tested against the analytic form directly.
  glm::dvec3 j2Acceleration(const glm::dvec3& r) const;

 private:
  Config config_;
};

}  // namespace core
