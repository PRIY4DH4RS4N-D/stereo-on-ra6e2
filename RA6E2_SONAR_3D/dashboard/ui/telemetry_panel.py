"""
telemetry_panel.py
==================
Real-time telemetry readout and sensor diagnostics panel.
"""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout, QLabel, QGroupBox, QFrame
)
from PySide6.QtCore import Qt

from dashboard.core.pointcloud_processor import Point3D


class TelemetryPanel(QWidget):
    """
    Displays real-time sensor measurements, XYZ coordinates, and session statistics.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(8)

        # 1. Real-time Measurement Box
        box_meas = QGroupBox("Live Ultrasound Telemetry")
        layout_meas = QGridLayout(box_meas)
        layout_meas.setSpacing(6)

        self.lbl_dist = self._create_value_label("-- mm", "#00e5ff", 20)
        self.lbl_echo = self._create_value_label("-- us", "#ffffff", 14)
        self.lbl_status = self._create_badge_label("DISCONNECTED", "#757575")

        layout_meas.addWidget(QLabel("Distance:"), 0, 0)
        layout_meas.addWidget(self.lbl_dist, 0, 1)
        layout_meas.addWidget(QLabel("Echo Time:"), 1, 0)
        layout_meas.addWidget(self.lbl_echo, 1, 1)
        layout_meas.addWidget(QLabel("Sensor Status:"), 2, 0)
        layout_meas.addWidget(self.lbl_status, 2, 1)

        layout.addWidget(box_meas)

        # 2. 3D Coordinates Box
        box_coords = QGroupBox("Cartesian Coordinates (mm)")
        layout_coords = QGridLayout(box_coords)
        layout_coords.setSpacing(6)

        self.lbl_x = self._create_value_label("X: --", "#ff5252", 15)
        self.lbl_y = self._create_value_label("Y: --", "#69f0ae", 15)
        self.lbl_z = self._create_value_label("Z: --", "#448aff", 15)

        layout_coords.addWidget(self.lbl_x, 0, 0)
        layout_coords.addWidget(self.lbl_y, 0, 1)
        layout_coords.addWidget(self.lbl_z, 0, 2)

        layout.addWidget(box_coords)

        # 3. Angular Orientation Box
        box_angles = QGroupBox("Sensor Orientation")
        layout_angles = QGridLayout(box_angles)
        layout_angles.setSpacing(6)

        self.lbl_azimuth = self._create_value_label("0.0°", "#ffd740", 16)
        self.lbl_elevation = self._create_value_label("0.0°", "#ffd740", 16)

        layout_angles.addWidget(QLabel("Azimuth (θ):"), 0, 0)
        layout_angles.addWidget(self.lbl_azimuth, 0, 1)
        layout_angles.addWidget(QLabel("Elevation (φ):"), 1, 0)
        layout_angles.addWidget(self.lbl_elevation, 1, 1)

        layout.addWidget(box_angles)

        # 4. Session Statistics Box
        box_stats = QGroupBox("Scan Statistics")
        layout_stats = QGridLayout(box_stats)
        layout_stats.setSpacing(6)

        self.lbl_total_points = QLabel("0")
        self.lbl_valid_points = QLabel("0")
        self.lbl_invalid_points = QLabel("0")

        layout_stats.addWidget(QLabel("Total Attempts:"), 0, 0)
        layout_stats.addWidget(self.lbl_total_points, 0, 1)
        layout_stats.addWidget(QLabel("Valid Points:"), 1, 0)
        layout_stats.addWidget(self.lbl_valid_points, 1, 1)
        layout_stats.addWidget(QLabel("Dropouts/Timeouts:"), 2, 0)
        layout_stats.addWidget(self.lbl_invalid_points, 2, 1)

        layout.addWidget(box_stats)
        layout.addStretch()

        self._total_count = 0
        self._valid_count = 0
        self._invalid_count = 0

    def _create_value_label(self, text: str, color: str, size: int) -> QLabel:
        lbl = QLabel(text)
        lbl.setStyleSheet(f"color: {color}; font-size: {size}px; font-weight: bold; font-family: monospace;")
        return lbl

    def _create_badge_label(self, text: str, bg_color: str) -> QLabel:
        lbl = QLabel(text)
        lbl.setAlignment(Qt.AlignCenter)
        lbl.setStyleSheet(
            f"background-color: {bg_color}; color: white; padding: 2px 8px; "
            f"border-radius: 4px; font-weight: bold; font-size: 11px;"
        )
        return lbl

    def update_measurement(self, point: Point3D):
        """Updates live labels from the latest captured point."""
        self._total_count += 1
        if point.is_valid:
            self._valid_count += 1
            self.lbl_dist.setText(f"{point.distance_mm:.1f} mm")
            self.lbl_echo.setText(f"{point.echo_us} μs")
            self.lbl_x.setText(f"X: {point.x:.1f}")
            self.lbl_y.setText(f"Y: {point.y:.1f}")
            self.lbl_z.setText(f"Z: {point.z:.1f}")
            self.lbl_status.setText("OK")
            self.lbl_status.setStyleSheet("background-color: #2e7d32; color: white; padding: 2px 8px; border-radius: 4px; font-weight: bold;")
        else:
            self._invalid_count += 1
            self.lbl_dist.setText("TIMEOUT")
            self.lbl_echo.setText("-- μs")
            self.lbl_status.setText(point.status.upper())
            self.lbl_status.setStyleSheet("background-color: #c62828; color: white; padding: 2px 8px; border-radius: 4px; font-weight: bold;")

        self.lbl_total_points.setText(str(self._total_count))
        self.lbl_valid_points.setText(str(self._valid_count))
        self.lbl_invalid_points.setText(str(self._invalid_count))

    def update_orientation_display(self, azimuth: float, elevation: float):
        self.lbl_azimuth.setText(f"{azimuth:.1f}°")
        self.lbl_elevation.setText(f"{elevation:+.1f}°")

    def reset_stats(self):
        self._total_count = 0
        self._valid_count = 0
        self._invalid_count = 0
        self.lbl_dist.setText("-- mm")
        self.lbl_echo.setText("-- us")
        self.lbl_x.setText("X: --")
        self.lbl_y.setText("Y: --")
        self.lbl_z.setText("Z: --")
        self.lbl_total_points.setText("0")
        self.lbl_valid_points.setText("0")
        self.lbl_invalid_points.setText("0")
