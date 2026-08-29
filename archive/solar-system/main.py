

import numpy as np
import matplotlib.pyplot as plt

# Units: AU, Solar masses, years.
# G = 4*pi^2 follows from kepler's 3rd law (T=1 yr, a=1, M=1 M_sun from Earth).
G = 4 * np.pi**2

positions = np.array([
    [0.0, 0.0],         # Sun [0]
    [1.0, 0.0],         # Earth [1]
])
velocities = np.array([
    [0.0, 0.0],         # Sun [0]
    [0.0, 2.0 * np.pi]  # Earth [1]
])
masses = np.array([1.0, 3.003e-6])  # Sun [0], Earth [1]

def compute_accelerations(positions, masses):
    n = len(masses)
    accelerations = np.zeros_like(positions)
    for i in range(n):
        for j in range(n):
            if i != j:
                r_vec = positions[j] - positions[i]
                r_mag = np.linalg.norm(r_vec)
                mass_factor = masses[j]
                accelerations[i] += G * mass_factor * r_vec / r_mag**3
    return accelerations


dt = 0.001  # around 8.8 hours
num_steps = 5000 # roughly 5 years

trajectory = np.zeros((num_steps, len(masses), 2))

for step in range(num_steps):
    trajectory[step] = positions
    accelerations = compute_accelerations(positions, masses)
    new_velocities = velocities + accelerations * dt
    new_positions = positions + velocities * dt
    velocities = new_velocities
    positions = new_positions

plt.plot(trajectory[:, 1, 0], trajectory[:, 1, 1], linewidth=0.6)
plt.plot(0, 0, 'yo', markersize=10)  # Sun
plt.gca().set_aspect('equal')
plt.xlabel('x (AU)')
plt.ylabel('y (AU)')
plt.title('Earth orbit — explicit Euler')
plt.grid(alpha=0.3)
plt.show()