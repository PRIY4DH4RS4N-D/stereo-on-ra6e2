"""
pointcloud_viewer.py
====================
Interactive 3D OpenGL Point Cloud Viewport using PyQtGraph OpenGL.
"""

import math
from typing import Optional
import numpy as np

from PySide6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QLabel
from PySide6.QtCore import Qt
import pyqtgraph.opengl as gl


class PointCloudViewer(QWidget):
    """
    High-performance 3D OpenGL viewport for visualizing sonar point clouds,
    measurement rays, sensor origin, and reference axes.
    """

    def __init__(self, parent=None):
        super().__init__(parent)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        # Create OpenGL View Widget
        self.view = gl.GLViewWidget()
        self.view.setBackgroundColor("#111625")  # Dark sci-fi theme
        self.view.setCameraPosition(distance=2500, elevation=25, azimuth=45)
        layout.addWidget(self.view)

        # Add Reference Grid on Ground (XZ plane at Y = -200 mm)
        self.grid = gl.GLGridItem()
        self.grid.setSize(x=4000, y=4000, z=0)
        self.grid.setSpacing(x=200, y=200, z=0)
        self.grid.translate(0, 0, -200)
        self.grid.setColor((50, 70, 100, 100))
        self.view.addItem(self.grid)

        # Add 3D Coordinate Axis Indicator
        self.axis = gl.GLAxisItem()
        self.axis.setSize(x=300, y=300, z=300)
        self.view.addItem(self.axis)

        # Sensor Origin Marker (Orange Sphere at (0,0,0))
        origin_pos = np.array([[0.0, 0.0, 0.0]], dtype=np.float32)
        origin_color = np.array([[1.0, 0.5, 0.0, 1.0]], dtype=np.float32)
        self.origin_marker = gl.GLScatterPlotItem(
            pos=origin_pos, color=origin_color, size=12, pxMode=True
        )
        self.view.addItem(self.origin_marker)

        # Aiming / Measurement Ray
        ray_pts = np.array([[0, 0, 0], [0, 0, 800]], dtype=np.float32)
        self.ray_line = gl.GLLinePlotItem(
            pos=ray_pts, color=(1.0, 0.2, 0.2, 0.8), width=2.0, antialias=True
        )
        self.view.addItem(self.ray_line)

        # Main Point Cloud Scatter Plot
        self.scatter = gl.GLScatterPlotItem(
            pos=np.empty((0, 3), dtype=np.float32),
            color=np.empty((0, 4), dtype=np.float32),
            size=6,
            pxMode=True
        )
        self.view.addItem(self.scatter)

        # Bottom View Toolbar
        toolbar = QHBoxLayout()
        toolbar.setContentsMargins(6, 2, 6, 4)

        btn_reset = QPushButton("Reset View")
        btn_reset.clicked.connect(self.reset_camera)
        toolbar.addWidget(btn_reset)

        btn_top = QPushButton("Top View")
        btn_top.clicked.connect(self.set_top_view)
        toolbar.addWidget(btn_top)

        btn_front = QPushButton("Front View")
        btn_front.clicked.connect(self.set_front_view)
        toolbar.addWidget(btn_front)

        btn_side = QPushButton("Side View")
        btn_side.clicked.connect(self.set_side_view)
        toolbar.addWidget(btn_side)

        toolbar.addStretch()
        self.lbl_info = QLabel("Points: 0")
        self.lbl_info.setStyleSheet("color: #8fa0b5; font-size: 11px;")
        toolbar.addWidget(self.lbl_info)

        layout.addLayout(toolbar)

    def update_point_cloud(self, xyz_array: np.ndarray, colors_array: np.ndarray):
        """
        Updates the 3D scatter plot with current accumulated points.
        """
        if len(xyz_array) == 0:
            self.scatter.setData(
                pos=np.empty((0, 3), dtype=np.float32),
                color=np.empty((0, 4), dtype=np.float32)
            )
            self.lbl_info.setText("Points: 0")
            return

        self.scatter.setData(pos=xyz_array, color=colors_array, size=7, pxMode=True)
        self.lbl_info.setText(f"Points: {len(xyz_array)}")

    def update_aiming_ray(self, azimuth_deg: float, elevation_deg: float, length_mm: float = 1000.0):
        """
        Draws active ray from origin in the sensor's current pointing direction.
        """
        az_rad = math.radians(azimuth_deg % 360.0)
        el_rad = math.radians(elevation_deg)

        dx = length_mm * math.cos(el_rad) * math.sin(az_rad)
        dy = length_mm * math.sin(el_rad)
        dz = length_mm * math.cos(el_rad) * math.cos(az_rad)

        pts = np.array([[0.0, 0.0, 0.0], [dx, dy, dz]], dtype=np.float32)
        self.ray_line.setData(pos=pts)

    def reset_camera(self):
        self.view.setCameraPosition(distance=2500, elevation=25, azimuth=45)

    def set_top_view(self):
        self.view.setCameraPosition(distance=2500, elevation=90, azimuth=0)

    def set_front_view(self):
        self.view.setCameraPosition(distance=2500, elevation=0, azimuth=0)

    def set_side_view(self):
        self.view.setCameraPosition(distance=2500, elevation=0, azimuth=90)
