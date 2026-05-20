

import numpy as np
import matplotlib.pyplot as plt

# Units: AU, Solar masses, years.
# G = 4*pi^2 follows from kepler's 3rd law (T=1 yr, a=1, M=1 M_sun from Earth).
G = 4 * np.pi**2

M_sun = 1.0
M_earth = 3.0e-6 # TODO: update this value to be more accurate once more planets are added.

# initial conditions\
# Earth
earth_pos = np.array([1.0, 0.0])
earth_vel = np.array([0.0, 2 * np.pi])  # 2*pi AU/year — circular orbit speed at r=1 AU
sun_pos = np.array([0.0, 0.0])
sun_vel = np.array([0.0, 0.0]) # stationary for now

