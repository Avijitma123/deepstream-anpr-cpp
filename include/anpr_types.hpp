#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace anpr {

struct BoundingBox {
    float left{0.0F};
    float top{0.0F};
    float width{0.0F};
    float height{0.0F};
};

struct PlateDetection {
    std::string camera_id;
    std::uint64_t tracking_id{0};
    BoundingBox bbox;
    float detector_confidence{0.0F};
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
};

struct OcrResult {
    std::string text;
    float confidence{0.0F};
};

struct PlateEvent {
    std::string camera_id;
    std::uint64_t tracking_id{0};
    std::string plate_text;
    float confidence{0.0F};
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
    BoundingBox bbox;
    std::string evidence_path;
};

}  // namespace anpr
