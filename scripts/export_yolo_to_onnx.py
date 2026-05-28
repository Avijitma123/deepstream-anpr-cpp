#!/usr/bin/env python3
"""Export a YOLO detector checkpoint to ONNX.

This script intentionally uses Ultralytics' public API because the detector
family and checkpoint format are model-specific. Install ultralytics in your
training/export environment, then run:

    python3 scripts/export_yolo_to_onnx.py weights/best.pt models/plate_detector.onnx
"""

from __future__ import annotations

import argparse
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export YOLO plate detector to ONNX")
    parser.add_argument("weights", type=Path, help="Path to YOLO .pt weights")
    parser.add_argument("output", type=Path, help="Destination ONNX path")
    parser.add_argument("--imgsz", type=int, default=640, help="Export image size")
    parser.add_argument("--opset", type=int, default=12, help="ONNX opset")
    parser.add_argument("--dynamic", action="store_true", help="Export dynamic input shapes")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        from ultralytics import YOLO
    except ImportError as exc:
        raise SystemExit("Install ultralytics first: pip install ultralytics") from exc

    args.output.parent.mkdir(parents=True, exist_ok=True)
    model = YOLO(str(args.weights))
    exported_path = Path(
        model.export(format="onnx", imgsz=args.imgsz, opset=args.opset, dynamic=args.dynamic)
    )
    exported_path.replace(args.output)
    print(f"Exported {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
