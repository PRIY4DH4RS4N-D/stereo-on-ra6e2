"""
main_window.py
==============
Master Application Window for RA6E2 SONAR-3D Ultrasonic Scanner.
"""

from PySide6.QtWidgets import (
    QMainWindow, QWidget, QHBoxLayout, QVBoxLayout, QTabWidget,
    QStatusBar, QLabel, QSplitter, QMessageBox
)
from PySide6.QtCore import Qt
from PySide6.QtGui import QKeySequence, QShortcut

from dashboard.core.serial_manager import SerialManager
from dashboard.core.pointcloud_processor import PointCloudProcessor, Point3D
from dashboard.core.scan_controller import ScanController
from dashboard.visualization.pointcloud_viewer import PointCloudViewer
from dashboard.ui.telemetry_panel import TelemetryPanel
from dashboard.ui.scanner_panel import ScannerPanel
from dashboard.ui.settings_panel import SettingsPanel


DARK_THEME_QSS = """
QMainWindow {
    background-color: #121824;
}
QWidget {
    background-color: #1a2234;
    color: #e0e6ed;
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    font-size: 12px;
}
QGroupBox {
    border: 1px solid #2d3b55;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 12px;
    font-weight: bold;
    color: #8fa0b5;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
    padding: 0 4px;
}
QTabWidget::pane {
    border: 1px solid #2d3b55;
    background-color: #1a2234;
}
QTabBar::tab {
    background: #141b29;
    color: #8fa0b5;
    padding: 8px 14px;
    border: 1px solid #243046;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
}
QTabBar::tab:selected {
    background: #232e44;
    color: #00e5ff;
    border-color: #00e5ff;
}
QPushButton {
    background-color: #26354f;
    border: 1px solid #3c527a;
    border-radius: 4px;
    color: #ffffff;
    padding: 6px 12px;
}
QPushButton:hover {
    background-color: #324669;
    border-color: #00e5ff;
}
QPushButton:pressed {
    background-color: #1e2b40;
}
QPushButton:disabled {
    background-color: #161e2c;
    color: #556677;
    border-color: #243046;
}
QSlider::groove:horizontal {
    height: 6px;
    background: #243046;
    border-radius: 3px;
}
QSlider::sub-page:horizontal {
    background: #00b0ff;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    background: #ffffff;
    width: 16px;
    margin-top: -5px;
    margin-bottom: -5px;
    border-radius: 8px;
}
QDoubleSpinBox, QSpinBox, QComboBox {
    background-color: #151d2c;
    border: 1px solid #2d3b55;
    border-radius: 4px;
    color: #ffffff;
    padding: 4px 6px;
}
QProgressBar {
    background-color: #151d2c;
    border: 1px solid #2d3b55;
    border-radius: 4px;
    text-align: center;
    color: white;
}
QProgressBar::chunk {
    background-color: #00e676;
    border-radius: 3px;
}
"""


class MainWindow(QMainWindow):
    """
    Main Application Window.
    """

    def __init__(self):
        super().__init__()
        self.setWindowTitle("RA6E2 SONAR-3D — Manual Ultrasonic 3D Point Cloud Scanner")
        self.resize(1300, 850)
        self.setStyleSheet(DARK_THEME_QSS)

        # Core Components
        self.serial_mgr = SerialManager(default_baud=115200)
        self.processor = PointCloudProcessor()
        self.controller = ScanController(self.serial_mgr, self.processor)

        # Main Central Splitter
        splitter = QSplitter(Qt.Horizontal)
        self.setCentralWidget(splitter)

        # Left Control Panels Container
        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)
        left_layout.setContentsMargins(6, 6, 6, 6)

        self.control_tabs = QTabWidget()

        # 1. Scanner Panel
        self.scanner_panel = ScannerPanel(self.controller)
        self.control_tabs.addTab(self.scanner_panel, "Scanning Controls")

        # 2. Telemetry Panel
        self.telemetry_panel = TelemetryPanel()
        self.control_tabs.addTab(self.telemetry_panel, "Telemetry & Stats")

        # 3. Settings Panel
        self.settings_panel = SettingsPanel(
            self.serial_mgr, self.processor, self.controller
        )
        self.control_tabs.addTab(self.settings_panel, "Hardware & Settings")

        left_layout.addWidget(self.control_tabs)
        splitter.addWidget(left_widget)

        # Right 3D Viewport
        self.viewer = PointCloudViewer()
        splitter.addWidget(self.viewer)

        # Set initial splitter proportion (35% controls, 65% 3D view)
        splitter.setSizes([450, 850])

        # Status Bar
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)

        self.lbl_status_conn = QLabel("Serial: Disconnected")
        self.lbl_status_mode = QLabel("Mode: Manual")
        self.lbl_status_cloud = QLabel("Cloud Points: 0")

        self.status_bar.addWidget(self.lbl_status_conn)
        self.status_bar.addPermanentWidget(self.lbl_status_mode)
        self.status_bar.addPermanentWidget(self.lbl_status_cloud)

        # Connect Signals
        self._setup_connections()

        # Keyboard Shortcut: Spacebar triggers Capture
        shortcut_space = QShortcut(QKeySequence(Qt.Key_Space), self)
        shortcut_space.activated.connect(self._on_space_pressed)

    def _setup_connections(self):
        # Point added by controller
        self.controller.point_added.connect(self._on_point_added)

        # Scanner panel orientation change -> update viewer ray and telemetry
        self.scanner_panel.orientation_changed.connect(self._on_orientation_changed)
        self.scanner_panel.capture_requested.connect(self.controller.capture_manual_point)

        # Serial status
        self.serial_mgr.connected.connect(
            lambda port: self.lbl_status_conn.setText(f"Serial: Connected ({port} @ 115200)")
        )
        self.serial_mgr.disconnected.connect(
            lambda: self.lbl_status_conn.setText("Serial: Disconnected")
        )
        self.serial_mgr.error_occurred.connect(
            lambda err: self.status_bar.showMessage(f"Error: {err}", 5000)
        )

        # Clear point cloud
        self.settings_panel.point_cloud_cleared.connect(self._on_cloud_cleared)
        self.settings_panel.session_imported.connect(self._on_session_imported)

    def _on_orientation_changed(self, az: float, el: float):
        self.viewer.update_aiming_ray(az, el, length_mm=1000.0)
        self.telemetry_panel.update_orientation_display(az, el)

    def _on_point_added(self, point: Point3D):
        self.telemetry_panel.update_measurement(point)
        # Refresh 3D viewer
        xyz = self.processor.get_xyz_array()
        colors = self.processor.get_colors_array()
        self.viewer.update_point_cloud(xyz, colors)
        self.lbl_status_cloud.setText(f"Cloud Points: {len(self.processor.points)}")

    def _on_cloud_cleared(self):
        self.viewer.update_point_cloud(
            self.processor.get_xyz_array(), self.processor.get_colors_array()
        )
        self.telemetry_panel.reset_stats()
        self.lbl_status_cloud.setText("Cloud Points: 0")

    def _on_session_imported(self):
        xyz = self.processor.get_xyz_array()
        colors = self.processor.get_colors_array()
        self.viewer.update_point_cloud(xyz, colors)
        self.lbl_status_cloud.setText(f"Cloud Points: {len(self.processor.points)}")

    def _on_space_pressed(self):
        """Spacebar triggers point capture or confirms grid alignment."""
        if self.scanner_panel.tabs.currentIndex() == 0:
            self.controller.capture_manual_point()
        else:
            self.controller.confirm_user_alignment()

    def closeEvent(self, event):
        self.serial_mgr.disconnect_port()
        event.accept()
