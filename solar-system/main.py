

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