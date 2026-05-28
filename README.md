# DeepStream ANPR C++ Project

An industry-level Automatic Number Plate Recognition system built using **NVIDIA DeepStream**, **C++**, **TensorRT**, and **GStreamer**.

This project is designed for real-time vehicle number plate detection, tracking, OCR recognition, event generation, and evidence storage from video files or RTSP camera streams.

---

## 1. Project Overview

The goal of this project is to build a production-grade ANPR pipeline using NVIDIA DeepStream on an Ubuntu system with an NVIDIA GPU.

The system takes live video input from an RTSP camera or video file, detects number plates, tracks detected plates across frames, extracts plate crops, performs OCR, applies post-processing rules, and stores valid recognition events.

The final output of the system includes:

- Detected number plate bounding boxes
- OCR-recognized plate text
- Plate confidence score
- Camera ID
- Timestamp
- Tracking ID
- Evidence image path
- Database/event log entry

---

## 2. Key Features

- Real-time ANPR using NVIDIA DeepStream
- C++ based high-performance implementation
- RTSP and video file input support
- TensorRT optimized inference
- License plate detection
- Object tracking using DeepStream tracker
- Plate crop extraction
- OCR recognition module
- Indian number plate format validation
- Duplicate event suppression
- Evidence image saving
- Database/event logging support
- Modular and scalable project architecture
- Future-ready design for multi-camera deployment

---

## 3. System Architecture

The complete ANPR pipeline follows this architecture:

```text
RTSP Camera / Video File
        |
        v
GStreamer Source
        |
        v
Video Decoder
        |
        v
nvstreammux
        |
        v
Primary Inference
License Plate Detector
        |
        v
nvtracker
        |
        v
Metadata Probe
        |
        v
Plate Crop Extraction
DeepStream Object Encoder
        |
        v
Persistent OCR Worker Process
TensorRT LPRNet
        |
        v
Plate Text Post-processing
        |
        v
Event Manager
        |
        v
Database / Evidence Storage / API
```

---

## 4. Current Implementation Status

The repository currently includes:

- A CMake-based C++ application: `deepstream-anpr`
- A separate OCR helper executable: `deepstream-anpr-ocr`
- DeepStream detector pipeline using `nvurisrcbin`, `nvstreammux`, `nvinfer`, `nvtracker`, `nvvideoconvert`, and `nvdsosd`
- Custom YOLO plate detector parser: `src/yolo_plate_parser.cpp`
- TensorRT LPRNet OCR engine wrapper: `src/ocr_engine.cpp`
- DeepStream metadata probe after `nvtracker`
- DeepStream object encoder crop saving into `evidence/`
- Persistent OCR worker process for selected crops
- CSV event writer and duplicate suppression

The full pipeline is operational:

```text
video/RTSP -> plate detector -> tracker -> padded crop save -> persistent OCR worker -> event CSV
```

The OCR engine is isolated in a separate executable because DeepStream 9 loads TensorRT 10 through `nvinfer`, while the LPRNet OCR path links TensorRT 11 on this machine. The full pipeline now starts this executable once in server mode and reuses it for crops, which avoids the old helper-per-crop startup cost while still avoiding TensorRT library conflicts inside one process.

---

## 5. System Requirements

Use an Ubuntu machine with an NVIDIA GPU and a working NVIDIA driver.

Tested project assumptions:

- Ubuntu Linux
- NVIDIA GPU
- CUDA installed under `/usr/local/cuda`
- TensorRT with `trtexec` available on `PATH`
- NVIDIA DeepStream 9.0 installed under:

```bash
/opt/nvidia/deepstream/deepstream-9.0
```

Required tools:

```bash
cmake
g++
make
trtexec
gst-launch-1.0
gst-inspect-1.0
convert
```

`convert` is provided by ImageMagick and is currently used by the OCR image preprocessing path.

Install missing build tools if needed:

```bash
sudo apt update
sudo apt install -y build-essential cmake imagemagick
```

DeepStream, CUDA, TensorRT, and NVIDIA driver installation should be done from NVIDIA's official packages for your GPU/driver/CUDA version.

---

## 6. Repository Setup

Clone the repository:

```bash
git clone git@github.com:Avijitma123/deepstream-anpr-cpp.git
cd deepstream-anpr-cpp
```

If you are using HTTPS instead of SSH:

```bash
git clone https://github.com/Avijitma123/deepstream-anpr-cpp.git
cd deepstream-anpr-cpp
```

Check that DeepStream plugins are visible to the system GStreamer:

```bash
GST_PLUGIN_PATH=/opt/nvidia/deepstream/deepstream-9.0/lib/gst-plugins \
LD_LIBRARY_PATH=/opt/nvidia/deepstream/deepstream-9.0/lib:/usr/local/cuda/lib64:${LD_LIBRARY_PATH} \
/usr/bin/gst-inspect-1.0 nvstreammux
```

If this prints plugin details for `nvstreammux`, DeepStream plugin discovery is working.

Important: if you use Conda, it may put its own `gst-launch-1.0` before the system binary. This project explicitly launches `/usr/bin/gst-launch-1.0` to avoid Conda GStreamer conflicts.

---

## 7. Model Files

Model binaries are intentionally ignored by Git. Put model files under `models/`.

Expected detector files:

```text
models/plate_detector.onnx
models/plate_detector.onnx_b1_gpu0_fp16.engine
models/labels.txt
```

Expected OCR files:

```text
models/us_lprnet_baseline18_deployable.onnx
models/us_lprnet_baseline18_deployable.engine
```

The current detector config uses:

```text
models/plate_detector.onnx_b1_gpu0_fp16.engine
```

The current OCR config uses:

```text
models/us_lprnet_baseline18_deployable.engine
```

If an engine file is missing, it can be generated from the ONNX file.

---

## 8. Build TensorRT Engines

Build all available engines:

```bash
./scripts/build_engine.sh
```

Or build the plate detector engine manually:

```bash
trtexec \
  --onnx=models/plate_detector.onnx \
  --saveEngine=models/plate_detector.onnx_b1_gpu0_fp16.engine \
  --fp16
```

Build the LPRNet OCR engine manually:

```bash
trtexec \
  --onnx=models/us_lprnet_baseline18_deployable.onnx \
  --saveEngine=models/us_lprnet_baseline18_deployable.engine \
  --minShapes=image_input:1x3x48x96 \
  --optShapes=image_input:1x3x48x96 \
  --maxShapes=image_input:1x3x48x96
```

Do not reuse TensorRT engines generated on a different TensorRT version, GPU architecture, or machine unless you know they are compatible. If DeepStream reports a serialization version mismatch, rebuild the engine from ONNX on the target machine.

---

## 9. Build The C++ Application

Configure and build:

```bash
cmake -S . -B build
cmake --build build
```

Expected outputs:

```text
build/deepstream-anpr
build/deepstream-anpr-ocr
build/libnvdsinfer_custom_yolo_plate.so
```

The shared library `libnvdsinfer_custom_yolo_plate.so` is loaded by DeepStream `nvinfer` through `configs/config_infer_plate_detector.txt`.

---

## 10. Run Full ANPR Pipeline

Run full pipeline on a video file without display:

```bash
./build/deepstream-anpr \
  --source /absolute/path/to/video.mp4 \
  --camera-id camera-01 \
  --no-display \
  --run
```

For quick local testing, cap OCR attempts:

```bash
./build/deepstream-anpr \
  --source /absolute/path/to/video.mp4 \
  --camera-id camera-01 \
  --no-display \
  --run \
  --max-ocr-attempts 5
```

Run full pipeline on a video file with display:

```bash
./build/deepstream-anpr \
  --source /absolute/path/to/video.mp4 \
  --camera-id camera-01 \
  --run
```

Run full pipeline on an RTSP stream:

```bash
./build/deepstream-anpr \
  --source rtsp://username:password@camera-ip:554/stream1 \
  --camera-id gate-01 \
  --no-display \
  --run
```

You can also use:

```bash
./scripts/run_rtsp.sh "rtsp://username:password@camera-ip:554/stream1"
```

During a successful run, the application:

1. Starts an in-process GStreamer/DeepStream pipeline
2. Detects plates with `nvinfer`
3. Tracks detections with `nvtracker`
4. Reads `NvDsObjectMeta` from a pad probe after tracker
5. Filters low-confidence or very small detections
6. Adds configurable padding around the detected plate box
7. Saves selected crops into `evidence/`
8. Sends crop paths to a persistent `build/deepstream-anpr-ocr --server` worker
9. Writes accepted events into `output/events.csv`

Example successful output:

```text
ANPR event: ABC1234 confidence=0.91 crop=evidence/camera-01_4_0_20260528T062138Z.jpg
EOS received - stopping full pipeline
Accepted events: 5, suppressed events: 0
```

A successful file run should end with EOS:

```text
EOS received - stopping full pipeline
```

Generated outputs:

```text
evidence/*.jpg
output/events.csv
```

Event CSV columns:

```text
timestamp,camera_id,tracking_id,plate_text,confidence,left,top,width,height,evidence_path
```

Runtime tuning options:

```text
--max-ocr-attempts <n>       Limit OCR attempts; 0 means unlimited
--crop-padding <ratio>       Add padding around each detector box before crop, default 0.20
--min-det-confidence <n>     Skip detections below this confidence, default 0.25
--min-crop-width <px>        Skip boxes narrower than this, default 24
--min-crop-height <px>       Skip boxes shorter than this, default 8
--ocr-binary <path>          OCR helper executable, default build/deepstream-anpr-ocr
```

---

## 11. Test OCR On One Plate Crop

After you have a cropped plate image, run:

```bash
./build/deepstream-anpr-ocr \
  --image /absolute/path/to/plate_crop.jpg
```

The OCR path:

1. Resizes the crop to `96x48`
2. Converts it to RGB
3. Runs TensorRT LPRNet inference
4. Decodes `tf_op_layer_ArgMax`
5. Prints the recognized text and confidence

Example output:

```text
ABC1234 confidence=0.91
```

If no OCR result is accepted, the command exits with code `2`.

The full pipeline calls this helper automatically in `--server` mode. You normally use the single-image command only when testing OCR quality on saved crops from `evidence/`.

---

## 12. Important Config Files

Detector config:

```text
configs/config_infer_plate_detector.txt
```

Key settings:

```text
model-engine-file=../models/plate_detector.onnx_b1_gpu0_fp16.engine
onnx-file=../models/plate_detector.onnx
labelfile-path=../models/labels.txt
custom-lib-path=../build/libnvdsinfer_custom_yolo_plate.so
parse-bbox-func-name=NvDsInferParseYoloPlate
output-blob-names=output0
infer-dims=3;640;640
num-detected-classes=1
```

OCR config:

```text
configs/config_ocr.txt
```

Key settings:

```text
model-engine-file=models/us_lprnet_baseline18_deployable.engine
onnx-file=models/us_lprnet_baseline18_deployable.onnx
input-layer=image_input
sequence-layer=tf_op_layer_ArgMax
confidence-layer=tf_op_layer_Max
alphabet=0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ
blank-index=36
input-width=96
input-height=48
min-confidence=0.60
```

Tracker config is currently passed directly to `nvtracker`:

```text
/opt/nvidia/deepstream/deepstream-9.0/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml
```

Full pipeline defaults:

```text
evidence-dir=evidence
events=output/events.csv
ocr-helper=build/deepstream-anpr-ocr
max-ocr-attempts=0
crop-padding=0.20
min-det-confidence=0.25
min-crop-width=24
min-crop-height=8
```

`max-ocr-attempts=0` means unlimited OCR attempts. Use `--max-ocr-attempts 5` or another small number during quick local tests.

---

## 13. Troubleshooting

### `Syntax error: "(" unexpected`

This happens when a GStreamer caps string such as `video/x-raw(memory:NVMM)` is passed through the shell without quoting. The project now quotes this internally.

### `no element "nvstreammux"`

DeepStream plugins are not visible to the GStreamer binary being used. Check:

```bash
which gst-launch-1.0
```

If it points to Conda, use `/usr/bin/gst-launch-1.0` or deactivate Conda. The project already uses `/usr/bin/gst-launch-1.0` internally.

Verify DeepStream plugin discovery:

```bash
GST_PLUGIN_PATH=/opt/nvidia/deepstream/deepstream-9.0/lib/gst-plugins \
LD_LIBRARY_PATH=/opt/nvidia/deepstream/deepstream-9.0/lib:/usr/local/cuda/lib64:${LD_LIBRARY_PATH} \
/usr/bin/gst-inspect-1.0 nvstreammux
```

### TensorRT serialization mismatch

If you see an error similar to:

```text
Serialization assertion stdVersionRead == kSERIALIZATION_VERSION failed
```

delete or ignore the old engine and rebuild it from ONNX on the same machine:

```bash
./scripts/build_engine.sh
```

### `Could not open custom lib`

Make sure the custom parser library exists:

```bash
ls -lh build/libnvdsinfer_custom_yolo_plate.so
```

If missing, rebuild:

```bash
cmake --build build
```

### `Failed to load OCR config`

Check that the OCR ONNX exists:

```bash
ls -lh models/us_lprnet_baseline18_deployable.onnx
```

Then build the OCR engine:

```bash
./scripts/build_engine.sh
```

### Full pipeline is slow

The full pipeline uses `build/deepstream-anpr-ocr --server` as a persistent external OCR worker to avoid TensorRT 10/11 conflicts with DeepStream. This is much faster than starting OCR separately for each crop, but still slower than a fully in-process OCR stage.

For the fastest production design, align DeepStream and OCR to the same TensorRT major version and move OCR in process. Until then, tune `--max-ocr-attempts`, `--min-det-confidence`, and crop size thresholds for your camera stream.

### OCR text is inaccurate

If crops are saved but OCR text is wrong:

1. Inspect the crop images in `evidence/`
2. Confirm the OCR model is trained for your target plate format
3. Tune `--crop-padding`, `--min-det-confidence`, and crop size filters
4. Check the configured alphabet and blank index in `configs/config_ocr.txt`

---

## 14. Final Workflow Notes

- Full detector -> tracker -> padded crop -> persistent OCR worker -> CSV event flow is implemented.
- OCR no longer starts a new process for every crop. The main app starts one OCR worker and streams crop paths to it.
- Crop quality is improved with configurable padding, detector confidence filtering, and minimum crop-size filtering.
- OCR attempts are unlimited by default. Use `--max-ocr-attempts <n>` only when you want a short local test.
- OCR is still out of process because of the local TensorRT major-version mismatch between DeepStream `nvinfer` and the OCR engine. Native in-process OCR requires both paths to use a compatible TensorRT runtime.
- OCR accuracy still depends on whether the LPRNet model, alphabet, blank index, and training data match the target license-plate format.
