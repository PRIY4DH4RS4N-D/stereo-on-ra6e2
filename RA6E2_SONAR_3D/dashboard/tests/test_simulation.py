"""
test_simulation.py
==================
Unit tests for the 3D acoustic room simulator.
"""

import pytest
from dashboard.simulation.acoustic_sim import AcousticSimulator


def test_flat_wall_simulation():
    sim = AcousticSimulator("Flat Wall (800 mm)")

    # Directly facing wall at Azimuth 0, Elevation 0
    dist, echo, status = sim.simulate_measurement(0.0, 0.0)
    assert status == "ok"
    assert 780 <= dist <= 820  # ~800 mm with Gaussian noise
    assert echo > 0

    # Facing away from wall (Azimuth 180 deg) -> should timeout
    dist_back, _, status_back = sim.simulate_measurement(180.0, 0.0)
    assert status_back == "timeout"
    assert dist_back == -1


def test_corner_simulation():
    sim = AcousticSimulator("Inside Corner (90 deg)")

    # Facing towards the corner (Azimuth 45 deg)
    dist, _, status = sim.simulate_measurement(45.0, 0.0)
    assert status == "ok"
    assert dist > 200


def test_enclosed_room():
    sim = AcousticSimulator("Enclosed Room (3m x 3m)")

    # Facing front
    dist_front, _, status_front = sim.simulate_measurement(0.0, 0.0)
    assert status_front == "ok"
    assert 2450 <= dist_front <= 2550

    # Facing right (Azimuth 90)
    dist_right, _, status_right = sim.simulate_measurement(90.0, 0.0)
    assert status_right == "ok"
    assert 1450 <= dist_right <= 1550
