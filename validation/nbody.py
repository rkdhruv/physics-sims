"""N-body forces and conserved quantities.

Units: AU, solar masses, years, which makes G = 4*pi^2.
2D throughout; the C++ engine in core/ is the 3D version.
"""

import numpy as np

G = 4 * np.pi**2


def compute_accelerations(positions, masses):
    """Gravitational acceleration on each body from every other body.

    a_i = sum_{j != i} G * m_j * (r_j - r_i) / |r_j - r_i|^3

    Args:
        positions: (n, 2) array in AU.
        masses:    (n,) array in solar masses.

    Returns:
        (n, 2) array of accelerations in AU/yr^2.
    """
    n = len(masses)
    accelerations = np.zeros_like(positions)
    for i in range(n):
        for j in range(n):
            if i != j:
                r_vec = positions[j] - positions[i]
                r_mag = np.linalg.norm(r_vec)
                accelerations[i] += G * masses[j] * r_vec / r_mag**3
    return accelerations


def total_energy(positions, velocities, masses):
    """Total mechanical energy: kinetic + potential.

        KE = sum_i (1/2) * m_i * |v_i|^2
        PE = -sum_{i<j} G * m_i * m_j / |r_i - r_j|

    Args:
        positions:  (n, 2) array in AU.
        velocities: (n, 2) array in AU/yr.
        masses:     (n,) array in solar masses.

    Returns:
        float, in M_sun * AU^2 / yr^2. Negative for a bound orbit.
    """
    n = len(masses)

    ke = 0.5 * np.sum(masses * np.sum(velocities**2, axis=1))

    pe = 0.0
    for i in range(n):
        for j in range(i + 1, n):  # unique pairs only
            dist = np.linalg.norm(positions[i] - positions[j])
            pe -= G * masses[i] * masses[j] / dist

    return float(ke + pe)


def angular_momentum(positions, velocities, masses):
    """Total angular momentum about the origin.

        L = sum_i m_i * (x_i * vy_i - y_i * vx_i)

    Args:
        positions:  (n, 2) array in AU.
        velocities: (n, 2) array in AU/yr.
        masses:     (n,) array in solar masses.

    Returns:
        float, in M_sun * AU^2 / yr. Signed: negative means clockwise.
    """
    # Written out rather than np.cross, which dropped 2D support in NumPy 2.x.
    x = positions[:, 0]
    y = positions[:, 1]
    vx = velocities[:, 0]
    vy = velocities[:, 1]

    return float(np.sum(masses * (x * vy - y * vx)))
