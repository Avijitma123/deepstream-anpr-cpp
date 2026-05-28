#include "ocr_engine.hpp"

#include <filesystem>
#include <utility>

namespace anpr {

OcrEngine::OcrEngine(OcrConfig config) : config_(std::move(config)) {}

bool OcrEngine::load() {
    loaded_ = config_.config_path.empty() || std::filesystem::exists(config_.config_path);
    return loaded_;
}

OcrResult OcrEngine::recognize(const PlateDetection&, const std::filesystem::path&) {
    if (!loaded_) {
        return {};
    }

    // TensorRT OCR decoding will be wired here after model-specific labels and bindings are finalized.
    return {};
}

}  // namespace anpr
