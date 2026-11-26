"""
scan_controller.py
==================
Controls manual and guided grid scanning workflows.
"""

from enum import Enum, auto
from typing import List, Tuple, Optional
import time

from PySide6.QtCore import QObject, Signal, QTimer

from dashboard.core.serial_manager import SerialManager
from dashboard.core.pointcloud_processor import PointCloudProcessor, Point3D
from dashboard.core.protocol import MeasurementResponse
from dashboard.simulation.acoustic_sim import AcousticSimulator


class ScanMode(Enum):
    MANUAL = auto()
    GUIDED_GRID = auto()


class ScanState(Enum):
    IDLE = auto()
    WAITING_FOR_USER = auto()
    MEASURING = auto()
    PAUSED = auto()
    FINISHED = auto()


class ScanController(QObject):
    """
    Coordinates scanning operations between the user, serial manager,
    and 3D point cloud processor.
    """

    # Signals
    target_changed = Signal(float, float, int, int)  # (azimuth, elevation, index, total)
    state_changed = Signal(ScanState)
    point_added = Signal(Point3D)
    scan_completed = Signal()
    progress_changed = Signal(int)  # 0 to 100%

    def __init__(self, serial_mgr: SerialManager, processor: PointCloudProcessor):
        super().__init__()
        self.serial_mgr = serial_mgr
        self.processor = processor

        self.mode = ScanMode.MANUAL
        self.state = ScanState.IDLE

        # Simulation mode toggle
        self.simulation_mode: bool = False
        self.sim = AcousticSimulator()

        # Current manual orientation
        self.current_azimuth_deg: float = 0.0
        self.current_elevation_deg: float = 0.0

        # Guided Grid parameters
        self.azimuth_min: float = -60.0
        self.azimuth_max: float = +60.0
        self.azimuth_step: float = 10.0

        self.elevation_min: float = -20.0
        self.elevation_max: float = +20.0
        self.elevation_step: float = 10.0

        self.auto_advance_sim: bool = True
        self.sim_delay_ms: int = 150

        # Grid plan
        self._grid_targets: List[Tuple[float, float]] = []
        self._current_target_idx: int = 0

        # Correlation
        self._active_request_id: Optional[int] = None

        # Connect serial manager signals
        self.serial_mgr.measurement_received.connect(self._on_measurement_received)

        # Simulation timer for auto-step
        self._sim_timer = QTimer(self)
        self._sim_timer.setSingleShot(True)
        self._sim_timer.timeout.connect(self._execute_simulated_step)

    def set_simulation_mode(self, enabled: bool):
        self.simulation_mode = enabled

    def set_simulation_scene(self, scene_name: str):
        self.sim.scene_name = scene_name

    def set_manual_orientation(self, azimuth: float, elevation: float):
        self.current_azimuth_deg = float(azimuth)
        self.current_elevation_deg = float(elevation)

    # --- Mode A: Manual Point Capture ---
    def capture_manual_point(self):
        """Triggers a single point capture at current manual orientation."""
        if self.simulation_mode:
            dist_mm, echo_us, status = self.sim.simulate_measurement(
                self.current_azimuth_deg, self.current_elevation_deg
            )
            resp = MeasurementResponse(
                request_id=999,
                distance_mm=dist_mm,
                echo_us=echo_us,
                status=status,
                raw_json="simulated"
            )
            point = self.processor.process_measurement(
                resp, self.current_azimuth_deg, self.current_elevation_deg
            )
            if point:
                self.point_added.emit(point)
        else:
            self._active_request_id = self.serial_mgr.send_measure_request()

    # --- Mode B: Guided Grid Scanning ---
    def configure_grid(
        self,
        az_min: float, az_max: float, az_step: float,
        el_min: float, el_max: float, el_step: float
    ):
        self.azimuth_min = az_min
        self.azimuth_max = az_max
        self.azimuth_step = max(1.0, az_step)
        self.elevation_min = el_min
        self.elevation_max = el_max
        self.elevation_step = max(1.0, el_step)

    def start_guided_scan(self):
        """Generates target grid and begins scanning sequence."""
        self._grid_targets.clear()

        # Build raster path (snake pattern for efficient manual orientation)
        el = self.elevation_min
        reverse_az = False
        while el <= self.elevation_max + 1e-4:
            az_range = []
            az = self.azimuth_min
            while az <= self.azimuth_max + 1e-4:
                az_range.append(az)
                az += self.azimuth_step

            if reverse_az:
                az_range.reverse()

            for a in az_range:
                self._grid_targets.append((round(a, 1), round(el, 1)))

            reverse_az = not reverse_az
            el += self.elevation_step

        self._current_target_idx = 0
        self.mode = ScanMode.GUIDED_GRID
        self._advance_to_next_target()

    def confirm_user_alignment(self):
        """Called when user confirms sensor is oriented to displayed target."""
        if self.state != ScanState.WAITING_FOR_USER:
            return

        self._set_state(ScanState.MEASURING)
        target_az, target_el = self._grid_targets[self._current_target_idx]

        if self.simulation_mode:
            self._sim_timer.start(50)
        else:
            self._active_request_id = self.serial_mgr.send_measure_request()

    def _execute_simulated_step(self):
        if self._current_target_idx >= len(self._grid_targets):
            self._set_state(ScanState.FINISHED)
            self.scan_completed.emit()
            return

        target_az, target_el = self._grid_targets[self._current_target_idx]
        dist_mm, echo_us, status = self.sim.simulate_measurement(target_az, target_el)

        resp = MeasurementResponse(
            request_id=1000 + self._current_target_idx,
            distance_mm=dist_mm,
            echo_us=echo_us,
            status=status,
            raw_json="simulated"
        )
        point = self.processor.process_measurement(resp, target_az, target_el)
        if point:
            self.point_added.emit(point)

        # Move to next target
        self._current_target_idx += 1
        progress = int((self._current_target_idx / len(self._grid_targets)) * 100)
        self.progress_changed.emit(progress)

        if self._current_target_idx < len(self._grid_targets):
            if self.auto_advance_sim:
                self._advance_to_next_target()
                self._sim_timer.start(self.sim_delay_ms)
            else:
                self._advance_to_next_target()
        else:
            self._set_state(ScanState.FINISHED)
            self.scan_completed.emit()

    def _advance_to_next_target(self):
        if self._current_target_idx < len(self._grid_targets):
            az, el = self._grid_targets[self._current_target_idx]
            self.current_azimuth_deg = az
            self.current_elevation_deg = el
            self._set_state(ScanState.WAITING_FOR_USER)
            self.target_changed.emit(az, el, self._current_target_idx + 1, len(self._grid_targets))
            progress = int((self._current_target_idx / len(self._grid_targets)) * 100)
            self.progress_changed.emit(progress)
        else:
            self._set_state(ScanState.FINISHED)
            self.scan_completed.emit()

    def stop_scan(self):
        self._sim_timer.stop()
        self._set_state(ScanState.IDLE)
        self.mode = ScanMode.MANUAL

    def _set_state(self, state: ScanState):
        self.state = state
        self.state_changed.emit(state)

    def _on_measurement_received(self, measurement: MeasurementResponse):
        """Handles response from physical serial port."""
        point = self.processor.process_measurement(
            measurement, self.current_azimuth_deg, self.current_elevation_deg
        )
        if point:
            self.point_added.emit(point)

        if self.mode == ScanMode.GUIDED_GRID and self.state == ScanState.MEASURING:
            self._current_target_idx += 1
            progress = int((self._current_target_idx / len(self._grid_targets)) * 100)
            self.progress_changed.emit(progress)
            self._advance_to_next_target()
