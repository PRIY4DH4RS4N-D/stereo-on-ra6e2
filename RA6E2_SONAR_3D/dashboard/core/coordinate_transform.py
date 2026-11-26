"""
coordinate_transform.py
========================
High-precision spherical to Cartesian coordinate transformation for 3D Sonar point clouds.

Coordinate Convention:
- Origin (0, 0, 0): Center of HC-SR04 ultrasonic sensor.
- r (range): Distance measured by ultrasonic sensor in millimeters (mm).
- theta (azimuth): Horizontal rotation angle in degrees [0, 360).
  - 0 deg   = Straight ahead (+Z axis)
  - 90 deg  = Right (+X axis)
  - 180 deg = Backward (-Z axis)
  - 270 deg = Left (-X axis)
- phi (elevation): Vertical tilt angle in degrees [-45, +45].
  - 0 deg   = Horizontal plane
  - +phi    = Tilted upward (+Y axis)
  - -phi    = Tilted downward (-Y axis)

Cartesian Conversion Equations:
- X = r * cos(phi) * sin(theta)  [Left-Right axis: +X is Right, -X is Left]
- Y = r * sin(phi)               [Vertical axis:   +Y is Up,    -Y is Down]
- Z = r * cos(phi) * cos(theta)  [Depth axis:      +Z is Front, -Z is Back]
"""

import math
from typing import Tuple, Union, Optional
import numpy as np


class CoordinateTransformer:
    """
    Transforms spherical coordinates (range, azimuth, elevation) into 3D Cartesian coordinates (X, Y, Z).
    """

    MIN_DISTANCE_MM: float = 20.0     # HC-SR04 physical min range: 2 cm
    MAX_DISTANCE_MM: float = 4000.0   # HC-SR04 physical max range: 400 cm
    MIN_ELEVATION_DEG: float = -90.0
    MAX_ELEVATION_DEG: float = +90.0

    @staticmethod
    def spherical_to_cartesian(
        distance_mm: float,
        azimuth_deg: float,
        elevation_deg: float,
        validate_range: bool = False
    ) -> Tuple[float, float, float]:
        """
        Convert a single spherical coordinate to Cartesian (X, Y, Z) in mm.

        :param distance_mm: Radial distance in mm.
        :param azimuth_deg: Azimuth angle in degrees [0..360).
        :param elevation_deg: Elevation angle in degrees [-90..+90].
        :param validate_range: If True, raises ValueError if distance is outside [20, 4000].
        :return: (x_mm, y_mm, z_mm) tuple rounded to 2 decimal places.
        """
        if validate_range:
            if not (CoordinateTransformer.MIN_DISTANCE_MM <= distance_mm <= CoordinateTransformer.MAX_DISTANCE_MM):
                raise ValueError(
                    f"Distance {distance_mm:.1f} mm outside valid range "
                    f"[{CoordinateTransformer.MIN_DISTANCE_MM}, {CoordinateTransformer.MAX_DISTANCE_MM}]"
                )

        # Normalize azimuth to [0, 360)
        norm_azimuth = azimuth_deg % 360.0

        # Clamp elevation to [-90, +90]
        clamped_elevation = max(CoordinateTransformer.MIN_ELEVATION_DEG,
                                min(CoordinateTransformer.MAX_ELEVATION_DEG, elevation_deg))

        # Convert to radians
        theta_rad = math.radians(norm_azimuth)
        phi_rad = math.radians(clamped_elevation)

        cos_phi = math.cos(phi_rad)
        sin_phi = math.sin(phi_rad)
        sin_theta = math.sin(theta_rad)
        cos_theta = math.cos(theta_rad)

        x = distance_mm * cos_phi * sin_theta
        y = distance_mm * sin_phi
        z = distance_mm * cos_phi * cos_theta

        # Guard against floating-point epsilon (e.g. 1e-15 instead of 0.0)
        x = 0.0 if abs(x) < 1e-10 else x
        y = 0.0 if abs(y) < 1e-10 else y
        z = 0.0 if abs(z) < 1e-10 else z

        return round(x, 2), round(y, 2), round(z, 2)

    @staticmethod
    def batch_spherical_to_cartesian(
        distances_mm: np.ndarray,
        azimuths_deg: np.ndarray,
        elevations_deg: np.ndarray
    ) -> np.ndarray:
        """
        Vectorized transformation of numpy arrays of coordinates.

        :return: (N, 3) ndarray containing [X, Y, Z] rows in mm.
        """
        distances_mm = np.asarray(distances_mm, dtype=np.float64)
        azimuths_deg = np.asarray(azimuths_deg, dtype=np.float64)
        elevations_deg = np.asarray(elevations_deg, dtype=np.float64)

        theta_rad = np.radians(azimuths_deg % 360.0)
        phi_rad = np.radians(np.clip(elevations_deg, -90.0, 90.0))

        cos_phi = np.cos(phi_rad)
        sin_phi = np.sin(phi_rad)
        sin_theta = np.sin(theta_rad)
        cos_theta = np.cos(theta_rad)

        x = distances_mm * cos_phi * sin_theta
        y = distances_mm * sin_phi
        z = distances_mm * cos_phi * cos_theta

        return np.column_stack((x, y, z))

    @staticmethod
    def cartesian_to_spherical(
        x: float, y: float, z: float
    ) -> Tuple[float, float, float]:
        """
        Reverse transformation: Cartesian (X, Y, Z) to spherical (r, azimuth_deg, elevation_deg).
        """
        r = math.sqrt(x * x + y * y + z * z)
        if r < 1e-9:
            return 0.0, 0.0, 0.0

        elevation_rad = math.asin(max(-1.0, min(1.0, y / r)))
        elevation_deg = math.degrees(elevation_rad)

        # Azimuth from Z and X
        azimuth_rad = math.atan2(x, z)
        azimuth_deg = math.degrees(azimuth_rad) % 360.0

        return round(r, 2), round(azimuth_deg, 2), round(elevation_deg, 2)
