"""
settings_panel.py
=================
Hardware port configuration, noise filter tuning, simulation scene selection,
and file import/export management.
"""

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout, QLabel, QPushButton,
    QComboBox, QCheckBox, QSlider, QDoubleSpinBox, QSpinBox, QGroupBox,
    QFileDialog, QMessageBox
)
from PySide6.QtCore import Qt, Signal

from dashboard.core.serial_manager import SerialManager
from dashboard.core.pointcloud_processor import PointCloudProcessor
from dashboard.core.scan_controller import ScanController
from dashboard.core.data_manager import DataManager
from dashboard.simulation.acoustic_sim import AcousticSimulator


class SettingsPanel(QWidget):
    """
    Settings panel for serial ports, digital filters, simulation, and file export.
    """

    point_cloud_cleared = Signal()
    session_imported = Signal()

    def __init__(
        self,
        serial_mgr: SerialManager,
        processor: PointCloudProcessor,
        controller: ScanController,
        parent=None
    ):
        super().__init__(parent)
        self.serial_mgr = serial_mgr
        self.processor = processor
        self.controller = controller

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(8)

        # 1. Serial Connection Box
        box_serial = QGroupBox("Hardware Interface (USB UART)")
        lay_serial = QGridLayout(box_serial)
        lay_serial.setSpacing(6)

        self.cb_ports = QComboBox()
        self._refresh_ports()
        btn_refresh = QPushButton("Refresh")
        btn_refresh.clicked.connect(self._refresh_ports)

        self.cb_baud = QComboBox()
        self.cb_baud.addItems(["115200", "57600", "9600", "230400"])
        self.cb_baud.setCurrentText("115200")

        self.btn_connect = QPushButton("CONNECT")
        self.btn_connect.setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 6px;")
        self.btn_connect.clicked.connect(self._toggle_connection)

        lay_serial.addWidget(QLabel("COM Port:"), 0, 0)
        lay_serial.addWidget(self.cb_ports, 0, 1)
        lay_serial.addWidget(btn_refresh, 0, 2)
        lay_serial.addWidget(QLabel("Baud Rate:"), 1, 0)
        lay_serial.addWidget(self.cb_baud, 1, 1)
        lay_serial.addWidget(self.btn_connect, 2, 0, 1, 3)

        layout.addWidget(box_serial)

        # 2. Simulation Mode Box
        box_sim = QGroupBox("Offline Simulation Mode")
        lay_sim = QVBoxLayout(box_sim)
        lay_sim.setSpacing(6)

        self.chk_sim = QCheckBox("Enable Acoustic Simulator (No Board Required)")
        self.chk_sim.setChecked(False)
        self.chk_sim.toggled.connect(self._on_sim_toggled)
        lay_sim.addWidget(self.chk_sim)

        lay_scene = QHBoxLayout()
        lay_scene.addWidget(QLabel("Scene:"))
        self.cb_scene = QComboBox()
        self.cb_scene.addItems(AcousticSimulator.SCENES)
        self.cb_scene.currentTextChanged.connect(self.controller.set_simulation_scene)
        lay_scene.addWidget(self.cb_scene)
        lay_sim.addLayout(lay_scene)

        self.chk_auto_sim = QCheckBox("Auto-Advance Guided Scan in Simulation")
        self.chk_auto_sim.setChecked(True)
        self.chk_auto_sim.toggled.connect(lambda v: setattr(self.controller, "auto_advance_sim", v))
        lay_sim.addWidget(self.chk_auto_sim)

        layout.addWidget(box_sim)

        # 3. Filter Tuning Box
        box_filter = QGroupBox("Digital Filtering & Smoothing")
        lay_filt = QGridLayout(box_filter)
        lay_filt.setSpacing(6)

        # Range filter
        lay_filt.addWidget(QLabel("Min/Max Range:"), 0, 0)
        self.spin_min_r = QSpinBox()
        self.spin_min_r.setRange(20, 1000)
        self.spin_min_r.setValue(int(self.processor.min_range_mm))
        self.spin_min_r.setSuffix(" mm")
        self.spin_min_r.valueChanged.connect(lambda v: setattr(self.processor, "min_range_mm", float(v)))

        self.spin_max_r = QSpinBox()
        self.spin_max_r.setRange(500, 5000)
        self.spin_max_r.setValue(int(self.processor.max_range_mm))
        self.spin_max_r.setSuffix(" mm")
        self.spin_max_r.valueChanged.connect(lambda v: setattr(self.processor, "max_range_mm", float(v)))

        lay_filt.addWidget(self.spin_min_r, 0, 1)
        lay_filt.addWidget(self.spin_max_r, 0, 2)

        # EMA Smoothing
        self.chk_ema = QCheckBox("EMA Distance Smoothing")
        self.chk_ema.setChecked(self.processor.ema_enabled)
        self.chk_ema.toggled.connect(lambda v: setattr(self.processor, "ema_enabled", v))
        lay_filt.addWidget(self.chk_ema, 1, 0, 1, 2)

        self.spin_alpha = QDoubleSpinBox()
        self.spin_alpha.setRange(0.05, 1.0)
        self.spin_alpha.setSingleStep(0.05)
        self.spin_alpha.setValue(self.processor.ema_alpha)
        self.spin_alpha.valueChanged.connect(lambda v: setattr(self.processor, "ema_alpha", float(v)))
        lay_filt.addWidget(self.spin_alpha, 1, 2)

        # Median Filter
        self.chk_median = QCheckBox("Median Window Filter")
        self.chk_median.setChecked(self.processor.median_filter_enabled)
        self.chk_median.toggled.connect(lambda v: setattr(self.processor, "median_filter_enabled", v))
        lay_filt.addWidget(self.chk_median, 2, 0, 1, 2)

        self.spin_median_win = QSpinBox()
        self.spin_median_win.setRange(3, 9)
        self.spin_median_win.setSingleStep(2)
        self.spin_median_win.setValue(self.processor.median_window_size)
        self.spin_median_win.valueChanged.connect(lambda v: setattr(self.processor, "median_window_size", int(v)))
        lay_filt.addWidget(self.spin_median_win, 2, 2)

        # Outlier Rejection
        self.chk_outlier = QCheckBox("Outlier Rejection Filter")
        self.chk_outlier.setChecked(self.processor.outlier_rejection_enabled)
        self.chk_outlier.toggled.connect(lambda v: setattr(self.processor, "outlier_rejection_enabled", v))
        lay_filt.addWidget(self.chk_outlier, 3, 0, 1, 3)

        layout.addWidget(box_filter)

        # 4. Export & Session Management Box
        box_export = QGroupBox("Data Export & Session")
        lay_exp = QGridLayout(box_export)
        lay_exp.setSpacing(6)

        btn_csv = QPushButton("Export CSV")
        btn_csv.clicked.connect(self._export_csv)
        lay_exp.addWidget(btn_csv, 0, 0)

        btn_ply = QPushButton("Export PLY (3D Mesh)")
        btn_ply.clicked.connect(self._export_ply)
        lay_exp.addWidget(btn_ply, 0, 1)

        btn_json = QPushButton("Export JSON Session")
        btn_json.clicked.connect(self._export_json)
        lay_exp.addWidget(btn_json, 1, 0)

        btn_import = QPushButton("Import Session")
        btn_import.clicked.connect(self._import_json)
        lay_exp.addWidget(btn_import, 1, 1)

        btn_clear = QPushButton("CLEAR POINT CLOUD")
        btn_clear.setStyleSheet("background-color: #c62828; color: white; font-weight: bold;")
        btn_clear.clicked.connect(self._clear_cloud)
        lay_exp.addWidget(btn_clear, 2, 0, 1, 2)

        layout.addWidget(box_export)
        layout.addStretch()

        # Connect serial manager signals
        self.serial_mgr.connected.connect(self._on_serial_connected)
        self.serial_mgr.disconnected.connect(self._on_serial_disconnected)

    def _refresh_ports(self):
        self.cb_ports.clear()
        ports = self.serial_mgr.list_available_ports()
        if "COM5" in ports:
            # Put COM5 first if present
            ports.remove("COM5")
            ports.insert(0, "COM5")
        self.cb_ports.addItems(ports)

    def _toggle_connection(self):
        if self.serial_mgr.is_connected():
            self.serial_mgr.disconnect_port()
        else:
            port = self.cb_ports.currentText()
            baud = int(self.cb_baud.currentText())
            if not port:
                QMessageBox.warning(self, "No Port", "Please select a valid COM port.")
                return
            self.serial_mgr.connect_port(port, baud)

    def _on_serial_connected(self, port: str):
        self.btn_connect.setText("DISCONNECT")
        self.btn_connect.setStyleSheet("background-color: #d32f2f; color: white; font-weight: bold;")
        self.chk_sim.setChecked(False)

    def _on_serial_disconnected(self):
        self.btn_connect.setText("CONNECT")
        self.btn_connect.setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold;")

    def _on_sim_toggled(self, checked: bool):
        self.controller.set_simulation_mode(checked)
        if checked and self.serial_mgr.is_connected():
            self.serial_mgr.disconnect_port()

    def _export_csv(self):
        if not self.processor.points:
            QMessageBox.information(self, "Empty Cloud", "No points to export.")
            return
        path, _ = QFileDialog.getSaveFileName(self, "Export CSV", "sonar_scan.csv", "CSV Files (*.csv)")
        if path:
            if DataManager.export_csv(path, self.processor.points):
                QMessageBox.information(self, "Success", f"Exported {len(self.processor.points)} points to CSV!")

    def _export_ply(self):
        if not self.processor.points:
            QMessageBox.information(self, "Empty Cloud", "No points to export.")
            return
        path, _ = QFileDialog.getSaveFileName(self, "Export PLY", "sonar_scan.ply", "PLY Point Cloud (*.ply)")
        if path:
            if DataManager.export_ply(path, self.processor.points):
                QMessageBox.information(self, "Success", f"Exported PLY file!")

    def _export_json(self):
        if not self.processor.points:
            QMessageBox.information(self, "Empty Cloud", "No points to export.")
            return
        path, _ = QFileDialog.getSaveFileName(self, "Export JSON Session", "sonar_session.json", "JSON (*.json)")
        if path:
            if DataManager.export_json(path, self.processor.points):
                QMessageBox.information(self, "Success", f"Session saved to JSON!")

    def _import_json(self):
        path, _ = QFileDialog.getOpenFileName(self, "Import JSON Session", "", "JSON (*.json)")
        if path:
            points = DataManager.import_json(path)
            if points is not None:
                self.processor.points = points
                self.session_imported.emit()
                QMessageBox.information(self, "Success", f"Loaded {len(points)} points from JSON!")

    def _clear_cloud(self):
        self.processor.clear()
        self.point_cloud_cleared.emit()
