#include "ocr_engine.hpp"

#include <NvInferRuntime.h>
#include <cuda_runtime_api.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace anpr {
namespace {

class TrtLogger final : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* message) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cerr << "[TensorRT OCR] " << message << '\n';
        }
    }
};

TrtLogger g_logger;

std::string trim(const std::string& value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (begin >= end) {
        return {};
    }
    return {begin, end};
}

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

std::size_t volume(const nvinfer1::Dims& dims) {
    std::size_t result = 1;
    for (int index = 0; index < dims.nbDims; ++index) {
        result *= static_cast<std::size_t>(dims.d[index]);
    }
    return result;
}

void checkCuda(cudaError_t error, const char* operation) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string(operation) + " failed: " + cudaGetErrorString(error));
    }
}

struct DeviceBuffer {
    void* ptr{nullptr};

    explicit DeviceBuffer(std::size_t bytes) {
        checkCuda(cudaMalloc(&ptr, bytes), "cudaMalloc");
    }

    ~DeviceBuffer() {
        if (ptr != nullptr) {
            cudaFree(ptr);
        }
    }

    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
};

std::vector<unsigned char> readPpm(const std::filesystem::path& path, int expected_width, int expected_height) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to read converted crop: " + path.string());
    }

    std::string magic;
    int width = 0;
    int height = 0;
    int max_value = 0;
    input >> magic >> width >> height >> max_value;
    input.get();

    if (magic != "P6" || width != expected_width || height != expected_height || max_value != 255) {
        throw std::runtime_error("unexpected PPM crop format");
    }

    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 3));
    input.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (input.gcount() != static_cast<std::streamsize>(pixels.size())) {
        throw std::runtime_error("incomplete PPM crop data");
    }
    return pixels;
}

}  // namespace

OcrEngine::OcrEngine(OcrConfig config) : config_(std::move(config)) {}

OcrEngine::~OcrEngine() = default;

bool OcrEngine::load() {
    if (!parseConfigFile() || !ensureEngineFile() || !loadEngine()) {
        loaded_ = false;
        return false;
    }
    loaded_ = true;
    return loaded_;
}

OcrResult OcrEngine::recognize(const PlateDetection&, const std::filesystem::path& crop_path) {
    if (!loaded_) {
        return {};
    }

    try {
        const auto input = loadImageTensor(crop_path);
        const auto input_name = config_.input_layer.c_str();
        const auto sequence_name = config_.sequence_layer.c_str();
        const auto confidence_name = config_.confidence_layer.c_str();

        const auto sequence_dims = engine_->getTensorShape(sequence_name);
        const auto confidence_dims = engine_->getTensorShape(confidence_name);
        const auto sequence_count = volume(sequence_dims);
        const auto confidence_count = volume(confidence_dims);

        std::vector<int> sequence(sequence_count);
        std::vector<float> confidence(confidence_count);

        DeviceBuffer input_device(input.size() * sizeof(float));
        DeviceBuffer sequence_device(sequence.size() * sizeof(int));
        DeviceBuffer confidence_device(confidence.size() * sizeof(float));

        cudaStream_t stream{};
        checkCuda(cudaStreamCreate(&stream), "cudaStreamCreate");

        checkCuda(cudaMemcpyAsync(input_device.ptr,
                                  input.data(),
                                  input.size() * sizeof(float),
                                  cudaMemcpyHostToDevice,
                                  stream),
                  "cudaMemcpyAsync input");

        context_->setTensorAddress(input_name, input_device.ptr);
        context_->setTensorAddress(sequence_name, sequence_device.ptr);
        context_->setTensorAddress(confidence_name, confidence_device.ptr);

        if (!context_->enqueueV3(stream)) {
            cudaStreamDestroy(stream);
            return {};
        }

        checkCuda(cudaMemcpyAsync(sequence.data(),
                                  sequence_device.ptr,
                                  sequence.size() * sizeof(int),
                                  cudaMemcpyDeviceToHost,
                                  stream),
                  "cudaMemcpyAsync sequence");
        checkCuda(cudaMemcpyAsync(confidence.data(),
                                  confidence_device.ptr,
                                  confidence.size() * sizeof(float),
                                  cudaMemcpyDeviceToHost,
                                  stream),
                  "cudaMemcpyAsync confidence");
        checkCuda(cudaStreamSynchronize(stream), "cudaStreamSynchronize");
        cudaStreamDestroy(stream);

        return decode(sequence, confidence);
    } catch (const std::exception& error) {
        std::cerr << "OCR failed for " << crop_path << ": " << error.what() << '\n';
        return {};
    }
}

bool OcrEngine::parseConfigFile() {
    if (config_.config_path.empty()) {
        return true;
    }

    std::ifstream input(config_.config_path);
    if (!input) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        if (key == "model-engine-file") {
            config_.engine_path = value;
        } else if (key == "onnx-file") {
            config_.onnx_path = value;
        } else if (key == "input-layer") {
            config_.input_layer = value;
        } else if (key == "sequence-layer") {
            config_.sequence_layer = value;
        } else if (key == "confidence-layer") {
            config_.confidence_layer = value;
        } else if (key == "alphabet") {
            config_.alphabet = value;
        } else if (key == "blank-index") {
            config_.blank_index = std::stoi(value);
        } else if (key == "input-width") {
            config_.input_width = std::stoi(value);
        } else if (key == "input-height") {
            config_.input_height = std::stoi(value);
        } else if (key == "min-confidence") {
            config_.min_confidence = std::stof(value);
        }
    }

    return true;
}

bool OcrEngine::ensureEngineFile() const {
    if (std::filesystem::exists(config_.engine_path) && std::filesystem::file_size(config_.engine_path) > 0) {
        return true;
    }

    if (!std::filesystem::exists(config_.onnx_path)) {
        std::cerr << "OCR ONNX file not found: " << config_.onnx_path << '\n';
        return false;
    }

    const std::string command =
        "trtexec --onnx=" + shellQuote(config_.onnx_path.string()) +
        " --saveEngine=" + shellQuote(config_.engine_path.string()) +
        " --minShapes=" + config_.input_layer + ":1x3x" + std::to_string(config_.input_height) + "x" +
        std::to_string(config_.input_width) + " --optShapes=" + config_.input_layer + ":1x3x" +
        std::to_string(config_.input_height) + "x" + std::to_string(config_.input_width) +
        " --maxShapes=" + config_.input_layer + ":1x3x" + std::to_string(config_.input_height) + "x" +
        std::to_string(config_.input_width);

    return std::system(command.c_str()) == 0 && std::filesystem::exists(config_.engine_path);
}

bool OcrEngine::loadEngine() {
    std::ifstream file(config_.engine_path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    runtime_.reset(nvinfer1::createInferRuntime(g_logger));
    if (!runtime_) {
        return false;
    }

    engine_.reset(runtime_->deserializeCudaEngine(data.data(), data.size()));
    if (!engine_) {
        return false;
    }

    context_.reset(engine_->createExecutionContext());
    if (!context_) {
        return false;
    }

    const nvinfer1::Dims4 input_shape{1, 3, config_.input_height, config_.input_width};
    return context_->setInputShape(config_.input_layer.c_str(), input_shape);
}

std::vector<float> OcrEngine::loadImageTensor(const std::filesystem::path& crop_path) const {
    const auto ppm_path = std::filesystem::temp_directory_path() / ("anpr_ocr_" + std::to_string(std::rand()) + ".ppm");
    const std::string command = "convert " + shellQuote(crop_path.string()) + " -resize " +
                                std::to_string(config_.input_width) + "x" +
                                std::to_string(config_.input_height) + "! -colorspace RGB " +
                                shellQuote("ppm:" + ppm_path.string());
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("ImageMagick convert failed");
    }

    const auto pixels = readPpm(ppm_path, config_.input_width, config_.input_height);
    std::filesystem::remove(ppm_path);

    std::vector<float> tensor(static_cast<std::size_t>(3 * config_.input_height * config_.input_width));
    const auto plane = static_cast<std::size_t>(config_.input_height * config_.input_width);
    for (int y = 0; y < config_.input_height; ++y) {
        for (int x = 0; x < config_.input_width; ++x) {
            const auto src = static_cast<std::size_t>((y * config_.input_width + x) * 3);
            const auto dst = static_cast<std::size_t>(y * config_.input_width + x);
            tensor[dst] = static_cast<float>(pixels[src]) / 255.0F;
            tensor[plane + dst] = static_cast<float>(pixels[src + 1]) / 255.0F;
            tensor[(2 * plane) + dst] = static_cast<float>(pixels[src + 2]) / 255.0F;
        }
    }
    return tensor;
}

OcrResult OcrEngine::decode(const std::vector<int>& sequence, const std::vector<float>& confidence) const {
    std::string text;
    float confidence_sum = 0.0F;
    int confidence_count = 0;
    int previous = config_.blank_index;

    for (std::size_t index = 0; index < sequence.size(); ++index) {
        const int symbol = sequence[index];
        if (symbol != config_.blank_index && symbol != previous && symbol >= 0 &&
            symbol < static_cast<int>(config_.alphabet.size())) {
            text.push_back(config_.alphabet[static_cast<std::size_t>(symbol)]);
            if (index < confidence.size()) {
                confidence_sum += confidence[index];
                ++confidence_count;
            }
        }
        previous = symbol;
    }

    OcrResult result;
    result.text = std::move(text);
    result.confidence = confidence_count > 0 ? confidence_sum / static_cast<float>(confidence_count) : 0.0F;
    if (result.confidence < config_.min_confidence) {
        return {};
    }
    return result;
}

}  // namespace anpr
