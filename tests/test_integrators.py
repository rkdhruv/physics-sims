"""Tests for the integrators and conserved quantities.

Run with: ./venv/bin/pytest
"""

import numpy as np
import pytest

from validation.integrators import (
    EXPECTED_ORDER,
    INTEGRATORS,
    euler,
)
from validation.nbody import G, angular_momentum, compute_accelerations, total_energy

# Aliased because pytest would otherwise collect the factory function
# `test_particle` as a test case.
from validation.scenarios import eccentric, sun_earth
from validation.scenarios import test_particle as make_test_particle
from validation.simulate import analytic_circular_position, integrate_only, run

ALL_INTEGRATORS = ["euler", "verlet", "rk4"]
GOOD_INTEGRATORS = ["verlet", "rk4"]  # everything but the deliberately-bad control


class TestConservedQuantities:

    def test_energy_matches_hand_computed_value(self):
        """Two unit masses 1 AU apart, one moving at 1 AU/yr: KE=0.5, PE=-G."""
        positions = np.array([[0.0, 0.0], [1.0, 0.0]])
        velocities = np.array([[0.0, 0.0], [0.0, 1.0]])
        masses = np.array([1.0, 1.0])

        expected = 0.5 - G
        assert total_energy(positions, velocities, masses) == pytest.approx(expected)

    def test_potential_is_not_double_counted(self):
        """Three equal masses, three unique pairs. Catches the i<j mistake,
        which a two-body case can hide."""
        positions = np.array([[0.0, 0.0], [1.0, 0.0], [0.0, 1.0]])
        velocities = np.zeros((3, 2))
        masses = np.array([1.0, 1.0, 1.0])

        # pairs: (0,1) at r=1, (0,2) at r=1, (1,2) at r=sqrt(2)
        expected = -G * (1.0 + 1.0 + 1.0 / np.sqrt(2))
        assert total_energy(positions, velocities, masses) == pytest.approx(expected)

    def test_bound_orbit_has_negative_energy(self):
        s = sun_earth()
        assert total_energy(s.positions, s.velocities, s.masses) < 0

    def test_angular_momentum_matches_hand_computed_value(self):
        """L = m * (x*vy - y*vx) = 1 * (1*1 - 0*0) = 1."""
        positions = np.array([[0.0, 0.0], [1.0, 0.0]])
        velocities = np.array([[0.0, 0.0], [0.0, 1.0]])
        masses = np.array([1.0, 1.0])

        assert angular_momentum(positions, velocities, masses) == pytest.approx(1.0)

    def test_angular_momentum_sign_flips_with_orbit_direction(self):
        positions = np.array([[0.0, 0.0], [1.0, 0.0]])
        velocities = np.array([[0.0, 0.0], [0.0, 1.0]])
        masses = np.array([1.0, 1.0])

        forward = angular_momentum(positions, velocities, masses)
        backward = angular_momentum(positions, -velocities, masses)
        assert forward == pytest.approx(-backward)


class TestInterface:

    @pytest.mark.parametrize("name", ALL_INTEGRATORS)
    def test_does_not_mutate_inputs(self, name):
        step = INTEGRATORS[name]
        s = sun_earth()
        pos_before = s.positions.copy()
        vel_before = s.velocities.copy()

        step(s.positions, s.velocities, s.masses, 0.001)

        np.testing.assert_array_equal(s.positions, pos_before)
        np.testing.assert_array_equal(s.velocities, vel_before)

    @pytest.mark.parametrize("name", ALL_INTEGRATORS)
    def test_returns_correct_shapes(self, name):
        step = INTEGRATORS[name]
        s = sun_earth()
        new_pos, new_vel = step(s.positions, s.velocities, s.masses, 0.001)

        assert new_pos.shape == s.positions.shape
        assert new_vel.shape == s.velocities.shape


class TestConvergenceOrder:
    """Global error scales as C * dt^p, so the slope of log(error) against
    log(dt) recovers the method's order p."""

    # Each method needs its own dt range to stay in the asymptotic regime.
    # Euler at dt=1e-2 has an error larger than the orbit itself.
    DT_SETS = {
        "euler": [1e-3, 5e-4, 2.5e-4],
        "verlet": [1e-2, 5e-3, 2.5e-3],
        "rk4": [1e-2, 5e-3, 2.5e-3],
    }

    @staticmethod
    def _measure_order(name, dts, duration=1.0):
        errors = []
        for dt in dts:
            positions, _ = integrate_only(make_test_particle(), name, dt, duration)
            exact = analytic_circular_position(duration)
            errors.append(np.linalg.norm(positions[1] - exact))

        slope, _ = np.polyfit(np.log(dts), np.log(errors), 1)
        return slope, errors

    @pytest.mark.parametrize("name", ALL_INTEGRATORS)
    def test_measured_order_matches_theory(self, name):
        expected = EXPECTED_ORDER[name]
        measured, errors = self._measure_order(name, self.DT_SETS[name])

        assert measured == pytest.approx(expected, abs=0.25), (
            f"{name}: expected order ~{expected}, measured {measured:.2f}. "
            f"errors at dt={self.DT_SETS[name]}: {errors}"
        )

    @pytest.mark.parametrize("name", ALL_INTEGRATORS)
    def test_error_decreases_with_smaller_dt(self, name):
        _, errors = self._measure_order(name, self.DT_SETS[name])
        assert errors == sorted(errors, reverse=True), (
            f"{name}: error did not decrease monotonically as dt shrank: {errors}"
        )

    def test_rk4_is_far_more_accurate_than_euler_at_equal_dt(self):
        dt, duration = 1e-3, 1.0
        exact = analytic_circular_position(duration)

        err = {}
        for name in ("euler", "rk4"):
            positions, _ = integrate_only(make_test_particle(), name, dt, duration)
            err[name] = np.linalg.norm(positions[1] - exact)

        assert err["rk4"] < err["euler"] / 1000


class TestOrbitStability:

    @pytest.mark.parametrize("name", GOOD_INTEGRATORS)
    def test_circular_orbit_keeps_its_radius(self, name):
        result = run(make_test_particle(), name, dt=1e-3, duration=10.0)
        radii = np.linalg.norm(result.trajectory[:, 1, :], axis=1)

        assert np.max(np.abs(radii - 1.0)) < 1e-3, (
            f"{name}: radius wandered to {radii.min():.6f}..{radii.max():.6f}"
        )

    def test_euler_spirals_outward(self):
        """The control group: Euler must visibly fail, or the diagnostics
        aren't detecting anything."""
        result = run(make_test_particle(), "euler", dt=1e-3, duration=10.0)
        radii = np.linalg.norm(result.trajectory[:, 1, :], axis=1)

        # Growth is a rising trend with oscillation on top, so the final sample
        # isn't necessarily the maximum. Compare per-orbit averages.
        steps_per_orbit = 1000
        first_orbit = np.mean(radii[:steps_per_orbit])
        last_orbit = np.mean(radii[-steps_per_orbit:])

        assert last_orbit > first_orbit * 1.05, (
            f"Euler should gain radius over 10 orbits: {first_orbit:.4f} -> {last_orbit:.4f}"
        )

    @pytest.mark.parametrize("name", GOOD_INTEGRATORS)
    def test_energy_drift_is_small(self, name):
        # sun_earth rather than test_particle: energy is mass-weighted, and the
        # test particle is massless, so E would be identically zero.
        result = run(sun_earth(), name, dt=1e-3, duration=10.0)
        assert np.max(result.energy_drift) < 1e-6

    def test_verlet_energy_error_is_bounded_not_growing(self):
        """Symplectic: the drift over the second half of the run is no worse
        than over the first half."""
        result = run(sun_earth(), "verlet", dt=1e-3, duration=20.0)
        drift = result.energy_drift
        midpoint = len(drift) // 2

        first_half_max = np.max(drift[:midpoint])
        second_half_max = np.max(drift[midpoint:])

        assert second_half_max < first_half_max * 1.5, (
            "Verlet energy error should stay bounded, not accumulate: "
            f"first half max {first_half_max:.3e}, second half max {second_half_max:.3e}"
        )

    # Measured, not guessed. Verlet conserves L to machine precision for a
    # central force; RK4 is merely very accurate.
    ANG_MOM_TOLERANCE = {"verlet": 1e-12, "rk4": 1e-9}

    @pytest.mark.parametrize("name", GOOD_INTEGRATORS)
    def test_angular_momentum_is_conserved(self, name):
        result = run(sun_earth(), name, dt=1e-3, duration=5.0)
        assert np.max(result.ang_momentum_drift) < self.ANG_MOM_TOLERANCE[name]

    def test_euler_loses_angular_momentum(self):
        """Euler leaks L as well as energy:

            L_{n+1} = m * (x_n + v_n*dt) x (v_n + a_n*dt)
                    = L_n + m*dt*(x_n x a_n) + m*dt^2*(v_n x a_n)

        The dt term vanishes for a central force, the dt^2 term doesn't, and it
        accumulates to ~14% over 5 years at dt=1e-3.
        """
        result = run(sun_earth(), "euler", dt=1e-3, duration=5.0)
        assert np.max(result.ang_momentum_drift) > 1e-3


class TestEccentricOrbit:

    @pytest.mark.parametrize("name", GOOD_INTEGRATORS)
    def test_returns_to_start_after_one_period(self, name):
        s = eccentric()
        positions, _ = integrate_only(s, name, dt=1e-4, duration=s.period)

        start = s.positions[1]
        assert np.linalg.norm(positions[1] - start) < 1e-3, (
            f"{name}: orbit did not close after one period"
        )

    @pytest.mark.parametrize("name", GOOD_INTEGRATORS)
    def test_perihelion_and_aphelion_are_consistent(self, name):
        """Aphelion is the 1 AU launch point; for e~0.36, a~0.735,
        r_peri = a(1-e) ~ 0.47 AU."""
        s = eccentric()
        result = run(s, name, dt=1e-4, duration=s.period)
        radii = np.linalg.norm(result.trajectory[:, 1, :], axis=1)

        assert np.max(radii) == pytest.approx(1.0, rel=1e-3)
        assert np.min(radii) == pytest.approx(0.47, rel=0.05)


def test_acceleration_obeys_inverse_square_law():
    """Doubling the separation quarters the acceleration."""
    masses = np.array([1.0, 0.0])

    a_near = compute_accelerations(np.array([[0.0, 0.0], [1.0, 0.0]]), masses)
    a_far = compute_accelerations(np.array([[0.0, 0.0], [2.0, 0.0]]), masses)

    assert np.linalg.norm(a_near[1]) == pytest.approx(
        4 * np.linalg.norm(a_far[1])
    )


def test_acceleration_points_toward_the_attractor():
    positions = np.array([[0.0, 0.0], [1.0, 0.0]])
    masses = np.array([1.0, 0.0])

    accelerations = compute_accelerations(positions, masses)
    assert accelerations[1][0] < 0


def test_euler_matches_the_scheme():
    """Pin euler() to the explicit scheme, so a refactor can't quietly
    change it into the semi-implicit variant."""
    s = sun_earth()
    dt = 0.001

    accelerations = compute_accelerations(s.positions, s.masses)
    expected_pos = s.positions + s.velocities * dt
    expected_vel = s.velocities + accelerations * dt

    new_pos, new_vel = euler(s.positions, s.velocities, s.masses, dt)

    np.testing.assert_allclose(new_pos, expected_pos)
    np.testing.assert_allclose(new_vel, expected_vel)
