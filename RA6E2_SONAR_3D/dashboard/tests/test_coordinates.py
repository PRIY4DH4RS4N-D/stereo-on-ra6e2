"""
test_coordinates.py
===================
Unit tests for coordinate transformation mathematics.
"""

import math
import pytest
import numpy as np
from dashboard.core.coordinate_transform import CoordinateTransformer


def test_cardinal_directions():
    r = 1000.0  # 1000 mm = 1 meter

    # 1. Front (Azimuth 0 deg, Elevation 0 deg) -> +Z
    x, y, z = CoordinateTransformer.spherical_to_cartesian(r, 0.0, 0.0)
    assert x == pytest.approx(0.0, abs=1e-2)
    assert y == pytest.approx(0.0, abs=1e-2)
    assert z == pytest.approx(1000.0, abs=1e-2)

    # 2. Right (Azimuth 90 deg, Elevation 0 deg) -> +X
    x, y, z = CoordinateTransformer.spherical_to_cartesian(r, 90.0, 0.0)
    assert x == pytest.approx(1000.0, abs=1e-2)
    assert y == pytest.approx(0.0, abs=1e-2)
    assert z == pytest.approx(0.0, abs=1e-2)

    # 3. Back (Azimuth 180 deg, Elevation 0 deg) -> -Z
    x, y, z = CoordinateTransformer.spherical_to_cartesian(r, 180.0, 0.0)
    assert x == pytest.approx(0.0, abs=1e-2)
    assert y == pytest.approx(0.0, abs=1e-2)
    assert z == pytest.approx(-1000.0, abs=1e-2)

    # 4. Left (Azimuth 270 deg, Elevation 0 deg) -> -X
    x, y, z = CoordinateTransformer.spherical_to_cartesian(r, 270.0, 0.0)
    assert x == pytest.approx(-1000.0, abs=1e-2)
    assert y == pytest.approx(0.0, abs=1e-2)
    assert z == pytest.approx(0.0, abs=1e-2)


def test_elevation_tilt():
    r = 1000.0

    # Tilted Up 30 deg, Front (Azimuth 0)
    # Y = 1000 * sin(30) = 500, Z = 1000 * cos(30) = 866.03
    x, y, z = CoordinateTransformer.spherical_to_cartesian(r, 0.0, 30.0)
    assert x == pytest.approx(0.0, abs=1e-2)
    assert y == pytest.approx(500.0, abs=1e-2)
    assert z == pytest.approx(866.03, abs=1e-2)

    # Tilted Down 45 deg, Right (Azimuth 90)
    # Y = 1000 * sin(-45) = -707.11, X = 1000 * cos(-45) = 707.11
    x, y, z = CoordinateTransformer.spherical_to_cartesian(r, 90.0, -45.0)
    assert x == pytest.approx(707.11, abs=1e-2)
    assert y == pytest.approx(-707.11, abs=1e-2)
    assert z == pytest.approx(0.0, abs=1e-2)


def test_batch_vectorization():
    distances = np.array([1000.0, 500.0, 800.0])
    azimuths = np.array([0.0, 90.0, 180.0])
    elevations = np.array([0.0, 0.0, 0.0])

    pts = CoordinateTransformer.batch_spherical_to_cartesian(distances, azimuths, elevations)
    assert pts.shape == (3, 3)

    assert pts[0, 2] == pytest.approx(1000.0, abs=1e-2)
    assert pts[1, 0] == pytest.approx(500.0, abs=1e-2)
    assert pts[2, 2] == pytest.approx(-800.0, abs=1e-2)


def test_cartesian_to_spherical_roundtrip():
    orig_r, orig_az, orig_el = 750.0, 45.0, 20.0
    x, y, z = CoordinateTransformer.spherical_to_cartesian(orig_r, orig_az, orig_el)

    rec_r, rec_az, rec_el = CoordinateTransformer.cartesian_to_spherical(x, y, z)
    assert rec_r == pytest.approx(orig_r, abs=1e-1)
    assert rec_az == pytest.approx(orig_az, abs=1e-1)
    assert rec_el == pytest.approx(orig_el, abs=1e-1)


def test_range_validation():
    with pytest.raises(ValueError):
        CoordinateTransformer.spherical_to_cartesian(10.0, 0.0, 0.0, validate_range=True)

    with pytest.raises(ValueError):
        CoordinateTransformer.spherical_to_cartesian(4500.0, 0.0, 0.0, validate_range=True)
