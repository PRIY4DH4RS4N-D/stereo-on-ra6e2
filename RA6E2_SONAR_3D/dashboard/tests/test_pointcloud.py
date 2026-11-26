"""
test_pointcloud.py
==================
Unit tests for point cloud accumulation, noise filtering, and data export.
"""

import os
import tempfile
import pytest
import numpy as np

from dashboard.core.pointcloud_processor import PointCloudProcessor, Point3D
from dashboard.core.protocol import MeasurementResponse
from dashboard.core.data_manager import DataManager


def test_pointcloud_accumulation_and_range_filter():
    proc = PointCloudProcessor()

    # Valid measurement
    m1 = MeasurementResponse(request_id=1, distance_mm=500, echo_us=2915, status="ok", raw_json="")
    p1 = proc.process_measurement(m1, azimuth_deg=0.0, elevation_deg=0.0)
    assert p1.is_valid is True
    assert len(proc.points) == 1
    assert p1.z == pytest.approx(500.0, abs=1e-1)

    # Invalid out of range (< 20 mm)
    m2 = MeasurementResponse(request_id=2, distance_mm=10, echo_us=58, status="ok", raw_json="")
    p2 = proc.process_measurement(m2, azimuth_deg=0.0, elevation_deg=0.0)
    assert p2.is_valid is False
    assert len(proc.points) == 1  # Not accumulated


def test_ema_filter():
    proc = PointCloudProcessor()
    proc.ema_enabled = True
    proc.ema_alpha = 0.5

    # First point: seeds the filter
    m1 = MeasurementResponse(request_id=1, distance_mm=500, echo_us=2915, status="ok", raw_json="")
    p1 = proc.process_measurement(m1, azimuth_deg=0.0, elevation_deg=0.0)
    assert p1.distance_mm == pytest.approx(500.0, abs=1e-1)

    # Second point with step change: 500 * 0.5 + 600 * 0.5 = 550
    m2 = MeasurementResponse(request_id=2, distance_mm=600, echo_us=3498, status="ok", raw_json="")
    p2 = proc.process_measurement(m2, azimuth_deg=0.0, elevation_deg=0.0)
    assert p2.distance_mm == pytest.approx(550.0, abs=1e-1)


def test_median_filter():
    proc = PointCloudProcessor()
    proc.ema_enabled = False
    proc.median_filter_enabled = True
    proc.median_window_size = 3

    # Feed sequence: 500, 1200 (spike), 510 -> median of window should reject spike
    proc.process_measurement(MeasurementResponse(1, 500, 0, "ok", ""), 0, 0)
    proc.process_measurement(MeasurementResponse(2, 1200, 0, "ok", ""), 0, 0)
    p3 = proc.process_measurement(MeasurementResponse(3, 510, 0, "ok", ""), 0, 0)

    # Median of [500, 1200, 510] is 510
    assert p3.distance_mm == pytest.approx(510.0, abs=1e-1)


def test_export_csv_and_ply():
    proc = PointCloudProcessor()
    proc.ema_enabled = False

    # Add sample points
    proc.process_measurement(MeasurementResponse(1, 500, 2915, "ok", ""), 0.0, 0.0)
    proc.process_measurement(MeasurementResponse(2, 800, 4664, "ok", ""), 90.0, 15.0)

    with tempfile.TemporaryDirectory() as tmpdir:
        csv_path = os.path.join(tmpdir, "test.csv")
        ply_path = os.path.join(tmpdir, "test.ply")
        json_path = os.path.join(tmpdir, "test.json")

        # 1. Test CSV Export
        assert DataManager.export_csv(csv_path, proc.points) is True
        assert os.path.exists(csv_path)
        with open(csv_path, "r", encoding="utf-8") as f:
            lines = f.readlines()
            assert len(lines) == 3  # Header + 2 data rows
            assert "x_mm,y_mm,z_mm" in lines[0]

        # 2. Test PLY Export
        assert DataManager.export_ply(ply_path, proc.points) is True
        assert os.path.exists(ply_path)
        with open(ply_path, "r", encoding="utf-8") as f:
            content = f.read()
            assert "element vertex 2" in content
            assert "property float x" in content
            assert "end_header" in content

        # 3. Test JSON Export and Import Roundtrip
        assert DataManager.export_json(json_path, proc.points, {"room": "lab"}) is True
        imported_points = DataManager.import_json(json_path)
        assert imported_points is not None
        assert len(imported_points) == 2
        assert imported_points[0].distance_mm == pytest.approx(500.0, abs=1e-1)
        assert imported_points[1].azimuth_deg == pytest.approx(90.0, abs=1e-1)
