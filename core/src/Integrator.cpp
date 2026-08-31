#include "core/Integrator.h"

#include <stdexcept>
#include <vector>

#include "core/ForceModel.h"

namespace core {
namespace {

// Scratch buffers, thread_local so they allocate once and get reused rather
// than malloc/free on every step.
struct Scratch {
  std::vector<glm::dvec3> a, a_next;
  std::vector<glm::dvec3> k1x, k1v, k2x, k2v, k3x, k3v, k4x, k4v;
  std::vector<glm::dvec3> temp_positions;

  // The kNv buffers are sized by the force model; these are written through
  // operator[], which neither bounds-checks nor grows, so they have to be
  // sized up front.
  void ensureSized(std::size_t n) {
    if (k2x.size() == n) return;
    k1x.assign(n, glm::dvec3(0.0));
    k2x.assign(n, glm::dvec3(0.0));
    k3x.assign(n, glm::dvec3(0.0));
    k4x.assign(n, glm::dvec3(0.0));
    temp_positions.assign(n, glm::dvec3(0.0));
  }
};

thread_local Scratch g;

// Explicit Euler. 1st order, 1 force evaluation per step.
//
//     x_{n+1} = x_n + v_n*dt
//     v_{n+1} = v_n + a_n*dt
//
// Positions must use the old velocities, so velocities are written second;
// swapping the loops gives semi-implicit Euler, a different method.
void stepEuler(System& s, double dt, const ForceModel& forces) {
  forces.accelerations(s, g.a);

  const std::size_t n = s.size();
  for (std::size_t i = 0; i < n; ++i) {
    s.positions[i] += s.velocities[i] * dt;
  }
  for (std::size_t i = 0; i < n; ++i) {
    s.velocities[i] += g.a[i] * dt;
  }
}

// Velocity Verlet. 2nd order, symplectic, 2 force evaluations per step.
//
//     a_n      = a(x_n)
//     x_{n+1}  = x_n + v_n*dt + (1/2)*a_n*dt^2
//     a_{n+1}  = a(x_{n+1})
//     v_{n+1}  = v_n + (1/2)*(a_n + a_{n+1})*dt
//
// Updating in place means a_n has to be finished with before anything it
// depends on is overwritten, hence three separate loops. Interleaving the
// position and velocity loops silently drops the method to first order.
void stepVerlet(System& s, double dt, const ForceModel& forces) {
  const std::size_t n = s.size();

  forces.accelerations(s, g.a);

  for (std::size_t i = 0; i < n; ++i) {
    s.positions[i] += s.velocities[i] * dt + 0.5 * g.a[i] * dt * dt;
  }

  forces.accelerations(s, g.a_next);

  for (std::size_t i = 0; i < n; ++i) {
    s.velocities[i] += 0.5 * (g.a[i] + g.a_next[i]) * dt;
  }
}

// Classical RK4. 4th order, 4 force evaluations per step.
//
// Applied to the stacked state y = (x, v), where f(y) = (v, a(x)):
//
//     k1 = f(y)
//     k2 = f(y + dt/2 * k1)
//     k3 = f(y + dt/2 * k2)
//     k4 = f(y + dt   * k3)
//     y_{n+1} = y_n + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
//
// g.temp_positions holds the intermediate positions each stage evaluates at.
void stepRK4(System& s, double dt, const ForceModel& forces) {
  const std::size_t n = s.size();
  g.ensureSized(n);

  // k1 = f(y)
  g.k1x = s.velocities;
  forces.accelerations(s, g.k1v);

  // k2 = f(y + dt/2 * k1)
  for (std::size_t i = 0; i < n; ++i) {
    g.k2x[i] = s.velocities[i] + 0.5 * dt * g.k1v[i];
    g.temp_positions[i] = s.positions[i] + 0.5 * dt * g.k1x[i];
  }
  forces.accelerations(g.temp_positions, s.masses, g.k2v);

  // k3 = f(y + dt/2 * k2)
  for (std::size_t i = 0; i < n; ++i) {
    g.k3x[i] = s.velocities[i] + 0.5 * dt * g.k2v[i];
    g.temp_positions[i] = s.positions[i] + 0.5 * dt * g.k2x[i];
  }
  forces.accelerations(g.temp_positions, s.masses, g.k3v);

  // k4 = f(y + dt * k3)
  for (std::size_t i = 0; i < n; ++i) {
    g.k4x[i] = s.velocities[i] + dt * g.k3v[i];
    g.temp_positions[i] = s.positions[i] + dt * g.k3x[i];
  }
  forces.accelerations(g.temp_positions, s.masses, g.k4v);

  // y_{n+1} = y_n + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)
  const double w = dt / 6.0;
  for (std::size_t i = 0; i < n; ++i) {
    s.positions[i] += w * (g.k1x[i] + 2.0 * g.k2x[i] + 2.0 * g.k3x[i] + g.k4x[i]);
    s.velocities[i] += w * (g.k1v[i] + 2.0 * g.k2v[i] + 2.0 * g.k3v[i] + g.k4v[i]);
  }
}

}  // namespace

void step(System& system, double dt, Method method, const ForceModel& forces) {
  switch (method) {
    case Method::Euler:  stepEuler(system, dt, forces);  return;
    case Method::Verlet: stepVerlet(system, dt, forces); return;
    case Method::RK4:    stepRK4(system, dt, forces);    return;
  }
  throw std::logic_error("unknown integration method");
}

const char* methodName(Method method) {
  switch (method) {
    case Method::Euler:  return "Explicit Euler";
    case Method::Verlet: return "Velocity Verlet";
    case Method::RK4:    return "RK4";
  }
  return "unknown";
}

int forceEvalsPerStep(Method method) {
  switch (method) {
    case Method::Euler:  return 1;
    case Method::Verlet: return 2;
    case Method::RK4:    return 4;
  }
  return 0;
}

}  // namespace core
