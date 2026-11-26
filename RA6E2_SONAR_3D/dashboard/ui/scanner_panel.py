"""
scanner_panel.py
================
Scanning controls for Manual Point Capture (Mode A) and Guided Grid Scanning (Mode B).
"""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout, QLabel, QPushButton,
    QSlider, QDoubleSpinBox, QTabWidget, QGroupBox, QProgressBar, QFrame
)
from PySide6.QtCore import Qt, Signal

from dashboard.core.scan_controller import ScanController, ScanState


class ScannerPanel(QWidget):
    """
    Control interface for manual orientation selection and guided grid execution.
    """

    orientation_changed = Signal(float, float)  # (azimuth, elevation)
    capture_requested = Signal()

    def __init__(self, controller: ScanController, parent=None):
        super().__init__(parent)
        self.controller = controller

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(6)

        # Tabs for Mode A vs Mode B
        self.tabs = QTabWidget()

        # Tab 1: Mode A - Manual Point Capture
        self.tab_manual = self._create_manual_tab()
        self.tabs.addTab(self.tab_manual, "Mode A: Manual Capture")

        # Tab 2: Mode B - Guided Grid Scanning
        self.tab_grid = self._create_grid_tab()
        self.tabs.addTab(self.tab_grid, "Mode B: Guided Grid")

        layout.addWidget(self.tabs)

        # Connect controller signals
        self.controller.target_changed.connect(self._on_target_changed)
        self.controller.state_changed.connect(self._on_state_changed)
        self.controller.progress_changed.connect(self.progress_bar.setValue)

    def _create_manual_tab(self) -> QWidget:
        tab = QWidget()
        layout = QVBoxLayout(tab)
        layout.setSpacing(10)

        # 1. Azimuth Control
        box_az = QGroupBox("Azimuth Angle (Horizontal: 0° to 360°)")
        lay_az = QHBoxLayout(box_az)

        self.slider_az = QSlider(Qt.Horizontal)
        self.slider_az.setRange(0, 360)
        self.slider_az.setValue(0)

        self.spin_az = QDoubleSpinBox()
        self.spin_az.setRange(0.0, 360.0)
        self.spin_az.setSingleStep(5.0)
        self.spin_az.setValue(0.0)
        self.spin_az.setSuffix("°")

        self.slider_az.valueChanged.connect(lambda v: self.spin_az.setValue(float(v)))
        self.spin_az.valueChanged.connect(lambda v: self.slider_az.setValue(int(v)))
        self.spin_az.valueChanged.connect(self._on_orientation_ui_changed)

        lay_az.addWidget(self.slider_az)
        lay_az.addWidget(self.spin_az)
        layout.addWidget(box_az)

        # Azimuth Presets
        box_presets = QHBoxLayout()
        for name, angle in [("Front (0°)", 0), ("Right (90°)", 90), ("Back (180°)", 180), ("Left (270°)", 270)]:
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked=False, a=angle: self.spin_az.setValue(a))
            box_presets.addWidget(btn)
        layout.addLayout(box_presets)

        # 2. Elevation Control
        box_el = QGroupBox("Elevation Angle (Vertical: -45° to +45°)")
        lay_el = QHBoxLayout(box_el)

        self.slider_el = QSlider(Qt.Horizontal)
        self.slider_el.setRange(-45, 45)
        self.slider_el.setValue(0)

        self.spin_el = QDoubleSpinBox()
        self.spin_el.setRange(-45.0, 45.0)
        self.spin_el.setSingleStep(5.0)
        self.spin_el.setValue(0.0)
        self.spin_el.setSuffix("°")

        self.slider_el.valueChanged.connect(lambda v: self.spin_el.setValue(float(v)))
        self.spin_el.valueChanged.connect(lambda v: self.slider_el.setValue(int(v)))
        self.spin_el.valueChanged.connect(self._on_orientation_ui_changed)

        lay_el.addWidget(self.slider_el)
        lay_el.addWidget(self.spin_el)
        layout.addWidget(box_el)

        # Big Capture Button
        self.btn_capture = QPushButton("CAPTURE POINT  [Space]")
        self.btn_capture.setStyleSheet(
            "background-color: #00b0ff; color: #000000; font-weight: bold; "
            "font-size: 15px; padding: 12px; border-radius: 6px;"
        )
        self.btn_capture.clicked.connect(self.capture_requested)
        layout.addWidget(self.btn_capture)
        layout.addStretch()

        return tab

    def _create_grid_tab(self) -> QWidget:
        tab = QWidget()
        layout = QVBoxLayout(tab)
        layout.setSpacing(8)

        # Grid Limits Box
        box_cfg = QGroupBox("Grid Bounds & Increments")
        grid_lay = QGridLayout(box_cfg)
        grid_lay.setSpacing(6)

        self.spin_az_min = QDoubleSpinBox()
        self.spin_az_min.setRange(-180, 180)
        self.spin_az_min.setValue(-50)
        self.spin_az_min.setSuffix("°")

        self.spin_az_max = QDoubleSpinBox()
        self.spin_az_max.setRange(-180, 180)
        self.spin_az_max.setValue(50)
        self.spin_az_max.setSuffix("°")

        self.spin_az_step = QDoubleSpinBox()
        self.spin_az_step.setRange(2, 45)
        self.spin_az_step.setValue(10)
        self.spin_az_step.setSuffix("°")

        self.spin_el_min = QDoubleSpinBox()
        self.spin_el_min.setRange(-45, 45)
        self.spin_el_min.setValue(-20)
        self.spin_el_min.setSuffix("°")

        self.spin_el_max = QDoubleSpinBox()
        self.spin_el_max.setRange(-45, 45)
        self.spin_el_max.setValue(20)
        self.spin_el_max.setSuffix("°")

        self.spin_el_step = QDoubleSpinBox()
        self.spin_el_step.setRange(2, 45)
        self.spin_el_step.setValue(10)
        self.spin_el_step.setSuffix("°")

        grid_lay.addWidget(QLabel("Azimuth Range:"), 0, 0)
        grid_lay.addWidget(self.spin_az_min, 0, 1)
        grid_lay.addWidget(QLabel("to"), 0, 2)
        grid_lay.addWidget(self.spin_az_max, 0, 3)
        grid_lay.addWidget(QLabel("Step:"), 0, 4)
        grid_lay.addWidget(self.spin_az_step, 0, 5)

        grid_lay.addWidget(QLabel("Elevation Range:"), 1, 0)
        grid_lay.addWidget(self.spin_el_min, 1, 1)
        grid_lay.addWidget(QLabel("to"), 1, 2)
        grid_lay.addWidget(self.spin_el_max, 1, 3)
        grid_lay.addWidget(QLabel("Step:"), 1, 4)
        grid_lay.addWidget(self.spin_el_step, 1, 5)

        layout.addWidget(box_cfg)

        # Target Guidance Guidance Banner
        box_guide = QGroupBox("Alignment Guidance")
        lay_guide = QVBoxLayout(box_guide)

        self.lbl_target = QLabel("Status: Idle. Click 'Start Grid Scan'")
        self.lbl_target.setStyleSheet("font-size: 13px; font-weight: bold; color: #ffd740;")
        self.lbl_target.setAlignment(Qt.AlignCenter)
        lay_guide.addWidget(self.lbl_target)

        self.btn_confirm = QPushButton("CONFIRM SENSOR POSITION")
        self.btn_confirm.setStyleSheet(
            "background-color: #00e676; color: #000000; font-weight: bold; "
            "font-size: 13px; padding: 10px; border-radius: 5px;"
        )
        self.btn_confirm.setEnabled(False)
        self.btn_confirm.clicked.connect(self.controller.confirm_user_alignment)
        lay_guide.addWidget(self.btn_confirm)

        layout.addWidget(box_guide)

        # Progress Bar
        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        layout.addWidget(self.progress_bar)

        # Start / Stop Buttons
        lay_btns = QHBoxLayout()
        self.btn_start_grid = QPushButton("START GRID SCAN")
        self.btn_start_grid.setStyleSheet("background-color: #2979ff; color: white; font-weight: bold; padding: 8px;")
        self.btn_start_grid.clicked.connect(self._on_start_grid_clicked)
        lay_btns.addWidget(self.btn_start_grid)

        self.btn_stop_grid = QPushButton("STOP SCAN")
        self.btn_stop_grid.setStyleSheet("background-color: #d50000; color: white; font-weight: bold; padding: 8px;")
        self.btn_stop_grid.setEnabled(False)
        self.btn_stop_grid.clicked.connect(self.controller.stop_scan)
        lay_btns.addWidget(self.btn_stop_grid)

        layout.addLayout(lay_btns)
        layout.addStretch()

        return tab

    def _on_orientation_ui_changed(self):
        az = self.spin_az.value()
        el = self.spin_el.value()
        self.orientation_changed.emit(az, el)
        self.controller.set_manual_orientation(az, el)

    def _on_start_grid_clicked(self):
        self.controller.configure_grid(
            self.spin_az_min.value(), self.spin_az_max.value(), self.spin_az_step.value(),
            self.spin_el_min.value(), self.spin_el_max.value(), self.spin_el_step.value()
        )
        self.controller.start_guided_scan()

    def _on_target_changed(self, az: float, el: float, idx: int, total: int):
        self.lbl_target.setText(f"Target [{idx}/{total}]: Align Sensor to  Azimuth: {az:.1f}° | Elevation: {el:+.1f}°")
        self.orientation_changed.emit(az, el)

    def _on_state_changed(self, state: ScanState):
        if state == ScanState.WAITING_FOR_USER:
            self.btn_confirm.setEnabled(True)
            self.btn_start_grid.setEnabled(False)
            self.btn_stop_grid.setEnabled(True)
        elif state == ScanState.MEASURING:
            self.btn_confirm.setEnabled(False)
        elif state in (ScanState.IDLE, ScanState.FINISHED):
            self.btn_confirm.setEnabled(False)
            self.btn_start_grid.setEnabled(True)
            self.btn_stop_grid.setEnabled(False)
            if state == ScanState.FINISHED:
                self.lbl_target.setText("Scan Completed! All points mapped.")
