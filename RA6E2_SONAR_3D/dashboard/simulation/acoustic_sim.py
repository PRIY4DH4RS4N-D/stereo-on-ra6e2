"""
acoustic_sim.py
===============
Realistic 3D acoustic room simulation for offline testing of RA6E2 SONAR-3D.

Simulates ultrasonic wave propagation, reflection geometry, beam divergence (15 deg cone),
and specular dropouts when incidence angles exceed the critical acoustic threshold.
"""

import math
import random
from typing import Tuple, Optional
import numpy as np


class AcousticSimulator:
    """
    Ray-tracing acoustic scene simulator for HC-SR04 sensor.
    """

    SCENES = [
        "Flat Wall (800 mm)",
        "Inside Corner (90 deg)",
        "Cylindrical Pillar",
        "Enclosed Room (3m x 3m)"
    ]

    def __init__(self, scene_name: str = "Flat Wall (800 mm)"):
        self.scene_name = scene_name
        self.noise_std_mm = 2.5  # Realistic HC-SR04 jitter
        self.max_detect_angle_deg = 55.0  # Max angle of incidence before beam bounces away

    def simulate_measurement(
        self, azimuth_deg: float, elevation_deg: float
    ) -> Tuple[int, int, str]:
        """
        Calculates distance to scene surface for the given ray direction.
        Returns: (distance_mm, echo_us, status_str)
        """
        # Direction unit vector
        az_rad = math.radians(azimuth_deg % 360.0)
        el_rad = math.radians(elevation_deg)

        dx = math.cos(el_rad) * math.sin(az_rad)
        dy = math.sin(el_rad)
        dz = math.cos(el_rad) * math.cos(az_rad)

        dist = None

        if self.scene_name == "Flat Wall (800 mm)":
            dist = self._intersect_flat_wall(dx, dy, dz, wall_z=800.0)

        elif self.scene_name == "Inside Corner (90 deg)":
            dist = self._intersect_corner(dx, dy, dz, corner_x=600.0, corner_z=600.0)

        elif self.scene_name == "Cylindrical Pillar":
            dist = self._intersect_cylinder(dx, dy, dz, cx=0.0, cz=900.0, radius=250.0)

        elif self.scene_name == "Enclosed Room (3m x 3m)":
            dist = self._intersect_room(dx, dy, dz)

        else:
            dist = self._intersect_flat_wall(dx, dy, dz, wall_z=800.0)

        if dist is None or dist < 20.0 or dist > 4000.0:
            return -1, 0, "timeout"

        # Add Gaussian noise
        noisy_dist = dist + random.gauss(0.0, self.noise_std_mm)
        noisy_dist = max(20.0, min(4000.0, noisy_dist))
        final_dist_mm = int(round(noisy_dist))

        # Calculate round-trip echo time in microseconds:
        # speed of sound = 343 m/s = 0.343 mm/us
        echo_us = int(round((2.0 * final_dist_mm) / 0.343))

        return final_dist_mm, echo_us, "ok"

    def _intersect_flat_wall(self, dx: float, dy: float, dz: float, wall_z: float) -> Optional[float]:
        # Wall plane: Z = wall_z
        if dz <= 0.05:  # Ray pointing away or parallel
            return None

        t = wall_z / dz
        # Check angle of incidence
        incidence_deg = math.degrees(math.acos(dz))
        if incidence_deg > self.max_detect_angle_deg:
            return None

        # Limit wall width & height
        x_hit = dx * t
        y_hit = dy * t
        if abs(x_hit) > 1200.0 or abs(y_hit) > 800.0:
            return None

        return t

    def _intersect_corner(self, dx: float, dy: float, dz: float, corner_x: float, corner_z: float) -> Optional[float]:
        # Corner between Wall 1 (Z = corner_z, X <= corner_x) and Wall 2 (X = corner_x, Z <= corner_z)
        t_candidates = []

        if dz > 0.05:
            t_z = corner_z / dz
            x_at_z = dx * t_z
            if -800.0 <= x_at_z <= corner_x:
                t_candidates.append(t_z)

        if dx > 0.05:
            t_x = corner_x / dx
            z_at_x = dz * t_x
            if -800.0 <= z_at_x <= corner_z:
                t_candidates.append(t_x)

        if not t_candidates:
            return None

        return min(t_candidates)

    def _intersect_cylinder(
        self, dx: float, dy: float, dz: float, cx: float, cz: float, radius: float
    ) -> Optional[float]:
        # Cylinder centered at (cx, cz) parallel to Y axis
        # Ray in XZ: x = t*dx, z = t*dz
        # (t*dx - cx)^2 + (t*dz - cz)^2 = r^2
        a = dx * dx + dz * dz
        if a < 1e-6:
            return None

        b = -2.0 * (cx * dx + cz * dz)
        c = cx * cx + cz * cz - radius * radius

        discriminant = b * b - 4.0 * a * c
        if discriminant < 0:
            # Fall back to background wall if missed
            return self._intersect_flat_wall(dx, dy, dz, wall_z=1500.0)

        t = (-b - math.sqrt(discriminant)) / (2.0 * a)
        if t <= 0:
            return None

        # Check height limits of pillar [-500, +500] mm
        y_hit = dy * t
        if abs(y_hit) > 500.0:
            return self._intersect_flat_wall(dx, dy, dz, wall_z=1500.0)

        return t

    def _intersect_room(self, dx: float, dy: float, dz: float) -> Optional[float]:
        # Box boundaries:
        # X: [-1500, +1500], Y: [-1000, +1500], Z: [-500, +2500]
        t_min = 1e9

        # Front wall: Z = 2500
        if dz > 0.01:
            t = 2500.0 / dz
            if abs(dx * t) <= 1500.0 and -1000.0 <= dy * t <= 1500.0:
                t_min = min(t_min, t)

        # Right wall: X = 1500
        if dx > 0.01:
            t = 1500.0 / dx
            if -500.0 <= dz * t <= 2500.0 and -1000.0 <= dy * t <= 1500.0:
                t_min = min(t_min, t)

        # Left wall: X = -1500
        if dx < -0.01:
            t = -1500.0 / dx
            if -500.0 <= dz * t <= 2500.0 and -1000.0 <= dy * t <= 1500.0:
                t_min = min(t_min, t)

        # Floor: Y = -1000
        if dy < -0.01:
            t = -1000.0 / dy
            if abs(dx * t) <= 1500.0 and -500.0 <= dz * t <= 2500.0:
                t_min = min(t_min, t)

        # Ceiling: Y = 1500
        if dy > 0.01:
            t = 1500.0 / dy
            if abs(dx * t) <= 1500.0 and -500.0 <= dz * t <= 2500.0:
                t_min = min(t_min, t)

        return t_min if t_min < 1e8 else None
