"""
data_manager.py
===============
Exports and imports 3D point clouds in CSV, PLY (Stanford format), and JSON session format.
"""

import os
import csv
import json
import time
from typing import List, Dict, Any, Optional

from dashboard.core.pointcloud_processor import Point3D, PointCloudProcessor


class DataManager:
    """
    Handles session persistence and standard 3D file exports.
    """

    @staticmethod
    def export_csv(filepath: str, points: List[Point3D]) -> bool:
        """
        Exports points to CSV format.
        """
        try:
            with open(filepath, mode="w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow([
                    "id", "timestamp", "request_id", "distance_mm", "raw_distance_mm",
                    "echo_us", "azimuth_deg", "elevation_deg", "x_mm", "y_mm", "z_mm",
                    "status", "is_valid"
                ])
                for p in points:
                    writer.writerow([
                        p.point_id, f"{p.timestamp:.3f}", p.request_id, p.distance_mm,
                        p.raw_distance_mm, p.echo_us, p.azimuth_deg, p.elevation_deg,
                        p.x, p.y, p.z, p.status, int(p.is_valid)
                    ])
            return True
        except Exception as e:
            print(f"[DataManager] Export CSV error: {e}")
            return False

    @staticmethod
    def export_ply(filepath: str, points: List[Point3D]) -> bool:
        """
        Exports valid points to standard Stanford PLY (ASCII) format with RGB color.
        Compatible with MeshLab, CloudCompare, and Blender.
        """
        valid_points = [p for p in points if p.is_valid]
        num_vertices = len(valid_points)

        try:
            with open(filepath, mode="w", encoding="utf-8") as f:
                # PLY Header
                f.write("ply\n")
                f.write("format ascii 1.0\n")
                f.write("comment Renesas BGK-RA6E2 SONAR-3D Point Cloud\n")
                f.write(f"element vertex {num_vertices}\n")
                f.write("property float x\n")
                f.write("property float y\n")
                f.write("property float z\n")
                f.write("property uchar red\n")
                f.write("property uchar green\n")
                f.write("property uchar blue\n")
                f.write("end_header\n")

                for p in valid_points:
                    r = int(p.color_rgb[0] * 255)
                    g = int(p.color_rgb[1] * 255)
                    b = int(p.color_rgb[2] * 255)
                    f.write(f"{p.x:.2f} {p.y:.2f} {p.z:.2f} {r} {g} {b}\n")
            return True
        except Exception as e:
            print(f"[DataManager] Export PLY error: {e}")
            return False

    @staticmethod
    def export_json(filepath: str, points: List[Point3D], metadata: Optional[Dict[str, Any]] = None) -> bool:
        """
        Exports full scan session including metadata and raw points to JSON.
        """
        if metadata is None:
            metadata = {}

        session_data = {
            "metadata": {
                "system": "RA6E2 SONAR-3D",
                "export_time": time.strftime("%Y-%m-%d %H:%M:%S"),
                "total_points": len(points),
                "valid_points": sum(1 for p in points if p.is_valid),
                **metadata
            },
            "points": [
                {
                    "id": p.point_id,
                    "timestamp": p.timestamp,
                    "request_id": p.request_id,
                    "distance_mm": p.distance_mm,
                    "raw_distance_mm": p.raw_distance_mm,
                    "echo_us": p.echo_us,
                    "azimuth_deg": p.azimuth_deg,
                    "elevation_deg": p.elevation_deg,
                    "x": p.x,
                    "y": p.y,
                    "z": p.z,
                    "status": p.status,
                    "is_valid": p.is_valid,
                    "color": list(p.color_rgb)
                }
                for p in points
            ]
        }

        try:
            with open(filepath, mode="w", encoding="utf-8") as f:
                json.dump(session_data, f, indent=2)
            return True
        except Exception as e:
            print(f"[DataManager] Export JSON error: {e}")
            return False

    @staticmethod
    def import_json(filepath: str) -> Optional[List[Point3D]]:
        """
        Loads a saved JSON scan session.
        """
        try:
            with open(filepath, mode="r", encoding="utf-8") as f:
                data = json.load(f)

            points: List[Point3D] = []
            for item in data.get("points", []):
                color = tuple(item.get("color", [0.2, 0.8, 1.0, 1.0]))
                p = Point3D(
                    point_id=item["id"],
                    timestamp=item["timestamp"],
                    request_id=item.get("request_id", 0),
                    distance_mm=item["distance_mm"],
                    raw_distance_mm=item.get("raw_distance_mm", item["distance_mm"]),
                    echo_us=item.get("echo_us", 0),
                    azimuth_deg=item["azimuth_deg"],
                    elevation_deg=item["elevation_deg"],
                    x=item["x"],
                    y=item["y"],
                    z=item["z"],
                    status=item.get("status", "ok"),
                    is_valid=item.get("is_valid", True),
                    color_rgb=color
                )
                points.append(p)
            return points
        except Exception as e:
            print(f"[DataManager] Import JSON error: {e}")
            return None
