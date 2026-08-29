"""Initial conditions with known analytic answers, for testing integrators."""

from dataclasses import dataclass

import numpy as np

from validation.nbody import G


@dataclass
class Scenario:
    name: str
    positions: np.ndarray   # (n, 2) AU
    velocities: np.ndarray  # (n, 2) AU/yr
    masses: np.ndarray      # (n,) M_sun
    period: float           # years
    description: str

    def copy(self):
        """Fresh arrays, so one run can't contaminate the next."""
        return Scenario(
            self.name,
            self.positions.copy(),
            self.velocities.copy(),
            self.masses.copy(),
            self.period,
            self.description,
        )


def sun_earth():
    """Sun + Earth on a circular 1 AU orbit, period 1 year.

    Circular velocity at radius r about mass M is sqrt(GM/r), which here is
    2*pi AU/yr -- one circumference per year.
    """
    return Scenario(
        name="sun_earth",
        positions=np.array([[0.0, 0.0], [1.0, 0.0]]),
        velocities=np.array([[0.0, 0.0], [0.0, 2.0 * np.pi]]),
        masses=np.array([1.0, 3.003e-6]),
        period=1.0,
        description="Sun + Earth, circular orbit at 1 AU",
    )


def test_particle():
    """Massless test particle on a circular orbit around a fixed Sun.

    Zero orbiting mass means the Sun feels no reaction force and stays at the
    origin, so the exact solution is (cos(2*pi*t), sin(2*pi*t)). Used by the
    convergence tests, which need exact truth to measure error against.

    Not usable for energy or angular momentum: both are mass-weighted sums and
    come out identically zero here.
    """
    return Scenario(
        name="test_particle",
        positions=np.array([[0.0, 0.0], [1.0, 0.0]]),
        velocities=np.array([[0.0, 0.0], [0.0, 2.0 * np.pi]]),
        masses=np.array([1.0, 0.0]),
        period=1.0,
        description="Massless test particle, exact circular orbit at 1 AU",
    )


def eccentric():
    """Elliptical orbit, e ~ 0.36, launched at aphelion with 80% of circular
    velocity.

    Harder on an integrator than a circular orbit, where a fixed timestep suits
    the whole path: on an ellipse the body moves fastest where the force is
    strongest, so a timestep that works at aphelion is too coarse at perihelion.

    From vis-viva, v^2 = GM(2/r - 1/a):
        specific energy  eps = v^2/2 - GM/r  = -0.68 * 4*pi^2
        semi-major axis  a   = -GM/(2*eps)   ~ 0.735 AU
        period           T   = a^1.5         ~ 0.630 yr
    """
    v_circ = 2.0 * np.pi
    v0 = 0.8 * v_circ
    r0 = 1.0

    specific_energy = 0.5 * v0**2 - G / r0
    a = -G / (2.0 * specific_energy)
    period = a**1.5

    # Earth's mass rather than zero, so the conserved quantities are non-zero
    # and this scenario stays usable for the conservation diagnostics.
    return Scenario(
        name="eccentric",
        positions=np.array([[0.0, 0.0], [r0, 0.0]]),
        velocities=np.array([[0.0, 0.0], [0.0, v0]]),
        masses=np.array([1.0, 3.003e-6]),
        period=period,
        description=f"Eccentric orbit, e~0.36, a={a:.3f} AU, T={period:.3f} yr",
    )


SCENARIOS = {
    "sun_earth": sun_earth,
    "test_particle": test_particle,
    "eccentric": eccentric,
}
