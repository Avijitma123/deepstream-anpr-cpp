#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="${1:-}"

if [[ -z "${SOURCE}" ]]; then
  if [[ -f "${ROOT_DIR}/configs/source_rtsp.txt" ]]; then
    SOURCE="$(grep -v '^#' "${ROOT_DIR}/configs/source_rtsp.txt" | head -n 1)"
  fi
fi

if [[ -z "${SOURCE}" ]]; then
  echo "Usage: $0 <rtsp-url|video-file>" >&2
  exit 1
fi

"${ROOT_DIR}/build/deepstream-anpr" \
  --source "${SOURCE}" \
  --camera-id camera-01 \
  --no-display \
  --run
