#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PLATE_ONNX="${ROOT_DIR}/models/plate_detector.onnx"
PLATE_ENGINE="${ROOT_DIR}/models/plate_detector.engine"
OCR_ONNX="${ROOT_DIR}/models/ocr.onnx"
OCR_ENGINE="${ROOT_DIR}/models/ocr.engine"
LPRNET_ONNX="${ROOT_DIR}/models/us_lprnet_baseline18_deployable.onnx"
LPRNET_ENGINE="${ROOT_DIR}/models/us_lprnet_baseline18_deployable.engine"

command -v trtexec >/dev/null 2>&1 || {
  echo "trtexec was not found. Install TensorRT and ensure trtexec is on PATH." >&2
  exit 1
}

if [[ -f "${PLATE_ONNX}" ]]; then
  trtexec --onnx="${PLATE_ONNX}" --saveEngine="${PLATE_ENGINE}" --fp16
else
  echo "Skipping plate detector: ${PLATE_ONNX} not found"
fi

if [[ -f "${OCR_ONNX}" ]]; then
  trtexec --onnx="${OCR_ONNX}" --saveEngine="${OCR_ENGINE}" --fp16
else
  echo "Skipping OCR: ${OCR_ONNX} not found"
fi

if [[ -f "${LPRNET_ONNX}" ]]; then
  trtexec \
    --onnx="${LPRNET_ONNX}" \
    --saveEngine="${LPRNET_ENGINE}" \
    --minShapes=image_input:1x3x48x96 \
    --optShapes=image_input:1x3x48x96 \
    --maxShapes=image_input:1x3x48x96
else
  echo "Skipping LPRNet OCR: ${LPRNET_ONNX} not found"
fi
