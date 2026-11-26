"""
pointcloud_processor.py
=======================
Point cloud data structures, filtering algorithms, and spatial accumulation.
"""

import time
from dataclasses import dataclass, field
from typing import List, Tuple, Optional, Dict
import numpy as np

from dashboard.core.coordinate_transform import CoordinateTransformer
from dashboard.core.protocol import MeasurementResponse


@dataclass
class Point3D:
    point_id: int
    timestamp: float
    request_id: int
    distance_mm: float
    raw_distance_mm: float
    echo_us: int
    azimuth_deg: float
    elevation_deg: float
    x: float
    y: float
    z: float
    status: str
    is_valid: bool
    color_rgb: Tuple[float, float, float, float] = (0.2, 0.8, 1.0, 1.0)  # RGBA normalized [0..1]


class PointCloudProcessor:
    """
    Manages point accumulation and real-time noise filtering.
    """

    def __init__(self):
        self.points: List[Point3D] = []
        self._next_id: int = 1

        # Filter parameters
        self.min_range_mm: float = 20.0
        self.max_range_mm: float = 4000.0
        self.ema_enabled: bool = True
        self.ema_alpha: float = 0.5  # Weight for new measurement [0.1..1.0]
        self._last_distance: Optional[float] = None

        # Multi-sample median buffer for static scanning
        self.median_filter_enabled: bool = False
        self.median_window_size: int = 3
        self._recent_measurements: List[float] = []

        # Outlier rejection
        self.outlier_rejection_enabled: bool = True
        self.max_jump_mm: float = 1200.0

    def clear(self):
        """Clears all accumulated points and resets filters."""
        self.points.clear()
        self._next_id = 1
        self._last_distance = None
        self._recent_measurements.clear()

    def process_measurement(
        self,
        measurement: MeasurementResponse,
        azimuth_deg: float,
        elevation_deg: float
    ) -> Optional[Point3D]:
        """
        Filters raw measurement, computes 3D coordinates, and stores point.
        """
        raw_dist = float(measurement.distance_mm)
        is_valid = measurement.is_valid

        # 1. Physical range check
        if not (self.min_range_mm <= raw_dist <= self.max_range_mm):
            is_valid = False

        filtered_dist = raw_dist

        if is_valid:
            # 2. Outlier rejection check (large sudden jumps)
            if self.outlier_rejection_enabled and self._last_distance is not None:
                if abs(filtered_dist - self._last_distance) > self.max_jump_mm:
                    # Mark as potential outlier but still track
                    pass

            # 3. Median filtering (if multiple consecutive reads at same spot)
            if self.median_filter_enabled:
                self._recent_measurements.append(filtered_dist)
                if len(self._recent_measurements) > self.median_window_size:
                    self._recent_measurements.pop(0)
                filtered_dist = float(np.median(self._recent_measurements))

            # 4. Exponential Moving Average (EMA) smoothing
            if self.ema_enabled and self._last_distance is not None:
                filtered_dist = (self.ema_alpha * filtered_dist) + ((1.0 - self.ema_alpha) * self._last_distance)

            self._last_distance = filtered_dist
        else:
            filtered_dist = -1.0

        # Calculate Cartesian coordinates if valid
        if is_valid and filtered_dist > 0:
            x, y, z = CoordinateTransformer.spherical_to_cartesian(
                filtered_dist, azimuth_deg, elevation_deg
            )
            color = self._compute_depth_color(filtered_dist)
        else:
            x, y, z = 0.0, 0.0, 0.0
            color = (0.5, 0.5, 0.5, 0.2)

        point = Point3D(
            point_id=self._next_id,
            timestamp=time.time(),
            request_id=measurement.request_id,
            distance_mm=round(filtered_dist, 2),
            raw_distance_mm=raw_dist,
            echo_us=measurement.echo_us,
            azimuth_deg=azimuth_deg,
            elevation_deg=elevation_deg,
            x=x,
            y=y,
            z=z,
            status=measurement.status if is_valid else "filtered_out",
            is_valid=is_valid,
            color_rgb=color
        )

        self._next_id += 1

        if is_valid:
            self.points.append(point)

        return point

    def _compute_depth_color(self, distance_mm: float) -> Tuple[float, float, float, float]:
        """
        Maps distance to Turbo/Viridis-like color gradient (normalized RGBA [0..1]).
        Near = Cyan/Green (0.2, 0.9, 0.4) -> Far = Blue/Magenta (0.9, 0.2, 0.6)
        """
        t = np.clip((distance_mm - self.min_range_mm) / (self.max_range_mm - self.min_range_mm), 0.0, 1.0)
        # Smooth polynomial approximation of Turbo colormap
        r = np.clip(1.5 - abs(4.0 * t - 3.0), 0.0, 1.0)
        g = np.clip(1.5 - abs(4.0 * t - 2.0), 0.0, 1.0)
        b = np.clip(1.5 - abs(4.0 * t - 1.0), 0.0, 1.0)
        return float(r), float(g), float(b), 0.85

    def get_xyz_array(self) -> np.ndarray:
        """
        Returns (N, 3) numpy array of valid point coordinates for 3D OpenGL rendering.
        """
        if not self.points:
            return np.empty((0, 3), dtype=np.float32)
        return np.array([[p.x, p.y, p.z] for p in self.points], dtype=np.float32)

    def get_colors_array(self) -> np.ndarray:
        """
        Returns (N, 4) numpy array of point RGBA colors.
        """
        if not self.points:
            return np.empty((0, 4), dtype=np.float32)
        return np.array([p.color_rgb for p in self.points], dtype=np.float32)
