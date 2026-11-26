"""
main.py
=======
Entry point for the RA6E2 SONAR-3D Desktop Dashboard.
"""

import sys
import os

# Ensure project root is in sys.path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from PySide6.QtWidgets import QApplication
from dashboard.ui.main_window import MainWindow


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("RA6E2 SONAR-3D Scanner")
    app.setOrganizationName("Renesas Robotics")

    window = MainWindow()
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
