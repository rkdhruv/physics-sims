"""Simulation driver: runs a scenario with a chosen integrator and records the
trajectory plus the conserved quantities at every step."""

from dataclasses import dataclass

import numpy as np

from validation.integrators import INTEGRATORS
from validation.nbody import angular_momentum, total_energy


@dataclass
class Run:
    """Everything recorded from one simulation."""

    integrator: str
    scenario: str
    dt: float
    times: np.ndarray         # (steps+1,) years
    trajectory: np.ndarray    # (steps+1, n, 2) AU
    energy: np.ndarray        # (steps+1,) total energy
    ang_momentum: np.ndarray  # (steps+1,) total angular momentum

    @property
    def energy_drift(self):
        """Relative energy error, |(E(t) - E_0) / E_0|. Relative so that runs
        with different scenarios are comparable."""
        return np.abs((self.energy - self.energy[0]) / self.energy[0])

    @property
    def ang_momentum_drift(self):
        return np.abs((self.ang_momentum - self.ang_momentum[0]) / self.ang_momentum[0])

    @property
    def final_positions(self):
        return self.trajectory[-1]


def run(scenario, integrator_name, dt, duration):
    """Integrate a scenario forward and record the full history.

    Args:
        scenario:        a Scenario (from validation.scenarios).
        integrator_name: key into INTEGRATORS -- "euler", "verlet", or "rk4".
        dt:              timestep in years.
        duration:        total time to simulate, in years.

    Returns:
        Run
    """
    step = INTEGRATORS[integrator_name]
    scenario = scenario.copy()

    positions = scenario.positions
    velocities = scenario.velocities
    masses = scenario.masses

    num_steps = int(round(duration / dt))
    n = len(masses)

    times = np.zeros(num_steps + 1)
    trajectory = np.zeros((num_steps + 1, n, 2))
    energy = np.zeros(num_steps + 1)
    ang_momentum = np.zeros(num_steps + 1)

    def record(k, t, pos, vel):
        times[k] = t
        trajectory[k] = pos
        energy[k] = total_energy(pos, vel, masses)
        ang_momentum[k] = angular_momentum(pos, vel, masses)

    record(0, 0.0, positions, velocities)

    for i in range(num_steps):
        positions, velocities = step(positions, velocities, masses, dt)
        record(i + 1, (i + 1) * dt, positions, velocities)

    return Run(
        integrator=integrator_name,
        scenario=scenario.name,
        dt=dt,
        times=times,
        trajectory=trajectory,
        energy=energy,
        ang_momentum=ang_momentum,
    )


def integrate_only(scenario, integrator_name, dt, duration):
    """Integrate without recording diagnostics; return final (positions, velocities).

    Used by the convergence tests, which sweep many timesteps and only need the
    endpoint. Skipping the per-step energy computation makes that much faster.
    """
    step = INTEGRATORS[integrator_name]
    scenario = scenario.copy()

    positions = scenario.positions
    velocities = scenario.velocities
    masses = scenario.masses

    for _ in range(int(round(duration / dt))):
        positions, velocities = step(positions, velocities, masses, dt)

    return positions, velocities


def analytic_circular_position(t, radius=1.0):
    """Exact position on a circular orbit at time t.

    Only valid for the `test_particle` scenario, where the central mass is
    fixed and the orbit is exactly circular with period 1 year.
    """
    omega = 2.0 * np.pi / 1.0
    return np.array([radius * np.cos(omega * t), radius * np.sin(omega * t)])
