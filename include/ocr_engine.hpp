#pragma once

#include "anpr_types.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace nvinfer1 {
class ICudaEngine;
class IExecutionContext;
class IRuntime;
}  // namespace nvinfer1

namespace anpr {

struct OcrConfig {
    std::filesystem::path config_path{"configs/config_ocr.txt"};
    std::filesystem::path engine_path{"models/us_lprnet_baseline18_deployable.engine"};
    std::filesystem::path onnx_path{"models/us_lprnet_baseline18_deployable.onnx"};
    std::string input_layer{"image_input"};
    std::string sequence_layer{"tf_op_layer_ArgMax"};
    std::string confidence_layer{"tf_op_layer_Max"};
    std::string alphabet{"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"};
    int blank_index{36};
    int input_width{96};
    int input_height{48};
    float min_confidence{0.60F};
};

class OcrEngine {
public:
    explicit OcrEngine(OcrConfig config);
    ~OcrEngine();

    bool load();
    OcrResult recognize(const PlateDetection& detection, const std::filesystem::path& crop_path);

private:
    bool parseConfigFile();
    bool ensureEngineFile() const;
    bool loadEngine();
    std::vector<float> loadImageTensor(const std::filesystem::path& crop_path) const;
    OcrResult decode(const std::vector<int>& sequence, const std::vector<float>& confidence) const;

    OcrConfig config_;
    bool loaded_{false};
    std::unique_ptr<nvinfer1::IRuntime> runtime_;
    std::unique_ptr<nvinfer1::ICudaEngine> engine_;
    std::unique_ptr<nvinfer1::IExecutionContext> context_;
};

}  // namespace anpr
