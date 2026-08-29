"""Time integrators.

All share one signature so they're interchangeable:

    step(positions, velocities, masses, dt) -> (new_positions, new_velocities)

None of them mutate their inputs.
"""

import numpy as np

from validation.nbody import compute_accelerations


def euler(positions, velocities, masses, dt):
    """Explicit (forward) Euler. 1st order, 1 force evaluation per step.

        a_n     = a(x_n)
        x_{n+1} = x_n + v_n * dt
        v_{n+1} = v_n + a_n * dt

    Both updates read old values, hence "explicit". Adds energy every step, so
    orbits spiral outward without bound; kept as the control group.
    """
    accelerations = compute_accelerations(positions, masses)
    new_positions = positions + velocities * dt
    new_velocities = velocities + accelerations * dt
    return new_positions, new_velocities


def velocity_verlet(positions, velocities, masses, dt):
    """Velocity Verlet. 2nd order, symplectic, 2 force evaluations per step.

        a_n      = a(x_n)
        x_{n+1}  = x_n + v_n*dt + (1/2)*a_n*dt^2
        a_{n+1}  = a(x_{n+1})
        v_{n+1}  = v_n + (1/2)*(a_n + a_{n+1})*dt

    Averaging the two accelerations is what makes it symplectic: energy error
    oscillates in a bounded band instead of accumulating.

    Returns:
        (new_positions, new_velocities)
    """
    curr_accel = compute_accelerations(positions, masses)
    new_positions = positions + velocities * dt + 0.5 * curr_accel * dt**2

    new_accel = compute_accelerations(new_positions, masses)
    new_velocities = velocities + 0.5 * (curr_accel + new_accel) * dt

    return new_positions, new_velocities


def rk4(positions, velocities, masses, dt):
    """Classical 4th-order Runge-Kutta. 4 force evaluations per step.

    Applied to the stacked state y = (x, v), where f(y) = (v, a(x)):

        k1 = f(y)
        k2 = f(y + dt/2 * k1)
        k3 = f(y + dt/2 * k2)
        k4 = f(y + dt   * k3)
        y_{n+1} = y_n + (dt/6) * (k1 + 2*k2 + 2*k3 + k4)

    Much more accurate per step than Verlet, but not symplectic, so its energy
    error drifts monotonically over long runs.

    Returns:
        (new_positions, new_velocities)
    """
    def deriv(x, v):
        return v, compute_accelerations(x, masses)

    k1_dx, k1_dv = deriv(positions, velocities)
    k2_dx, k2_dv = deriv(positions + dt * 0.5 * k1_dx, velocities + dt * 0.5 * k1_dv)
    k3_dx, k3_dv = deriv(positions + dt * 0.5 * k2_dx, velocities + dt * 0.5 * k2_dv)
    k4_dx, k4_dv = deriv(positions + dt * k3_dx, velocities + dt * k3_dv)

    new_positions = positions + (dt / 6) * (k1_dx + 2 * k2_dx + 2 * k3_dx + k4_dx)
    new_velocities = velocities + (dt / 6) * (k1_dv + 2 * k2_dv + 2 * k3_dv + k4_dv)

    return new_positions, new_velocities


INTEGRATORS = {
    "euler": euler,
    "verlet": velocity_verlet,
    "rk4": rk4,
}

#: Theoretical global order of accuracy, measured empirically by the test suite.
EXPECTED_ORDER = {
    "euler": 1,
    "verlet": 2,
    "rk4": 4,
}

#: Used to compare methods at equal cost rather than equal dt.
FORCE_EVALS_PER_STEP = {
    "euler": 1,
    "verlet": 2,
    "rk4": 4,
}
