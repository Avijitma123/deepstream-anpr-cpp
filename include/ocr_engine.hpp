#pragma once

#include "anpr_types.hpp"

#include <filesystem>
#include <string>

namespace anpr {

struct OcrConfig {
    std::filesystem::path config_path{"configs/config_ocr.txt"};
    float min_confidence{0.60F};
};

class OcrEngine {
public:
    explicit OcrEngine(OcrConfig config);

    bool load();
    OcrResult recognize(const PlateDetection& detection, const std::filesystem::path& crop_path);

private:
    OcrConfig config_;
    bool loaded_{false};
};

}  // namespace anpr
