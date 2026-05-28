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
        |
        v
OCR Engine
        |
        v
Plate Text Post-processing
        |
        v
Event Manager
        |
        v
Database / Evidence Storage / API
