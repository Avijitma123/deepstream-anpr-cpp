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
OCR Helper Process
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
- OCR helper execution for each selected crop
- CSV event writer and duplicate suppression

The full pipeline is operational:

```text
video/RTSP -> plate detector -> tracker -> crop save -> OCR helper -> event CSV
```

The current OCR helper is isolated in a separate executable because DeepStream 9 loads TensorRT 10 through `nvinfer`, while the LPRNet OCR path links TensorRT 11 on this machine. Keeping OCR in a helper process avoids TensorRT library conflicts inside one process.

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
5. Saves selected crops into `evidence/`
6. Calls `build/deepstream-anpr-ocr` for OCR
7. Writes accepted events into `output/events.csv`

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

The full pipeline calls this helper automatically. You normally use this command only when testing OCR quality on saved crops from `evidence/`.

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
max-ocr-attempts=5
```

The default OCR attempt cap keeps sample runs predictable because spawning a TensorRT OCR helper for every detection on every frame is slow. Increase this in `include/full_pipeline.hpp` when you move to a persistent OCR worker or in-process TensorRT-compatible OCR path.

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

The current full pipeline uses `build/deepstream-anpr-ocr` as an external OCR helper to avoid TensorRT 10/11 conflicts with DeepStream. This is correct for compatibility, but slower than an in-process OCR engine.

The default `max_ocr_attempts` is capped in `include/full_pipeline.hpp`. For production, replace the helper-per-crop call with a persistent OCR worker process or align DeepStream and OCR to the same TensorRT major version.

### OCR text is inaccurate

If crops are saved but OCR text is wrong:

1. Inspect the crop images in `evidence/`
2. Confirm the OCR model is trained for your target plate format
3. Tune crop padding and resize preprocessing
4. Check the configured alphabet and blank index in `configs/config_ocr.txt`

---

## 14. Current Limitations

- Full detector -> tracker -> crop -> OCR -> CSV event flow is implemented.
- OCR currently runs as a helper process for TensorRT compatibility, so it is slower than a native in-process OCR stage.
- OCR quality depends heavily on crop quality and whether the LPRNet model matches the target plate format.
- The default full-pipeline run limits OCR attempts to keep local testing fast.
