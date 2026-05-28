#include "nvdsinfer_custom_impl.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

constexpr int kBoxValues = 5;
constexpr float kNmsThreshold = 0.45F;

float clamp(float value, float low, float high) {
    return std::max(low, std::min(value, high));
}

float iou(const NvDsInferObjectDetectionInfo& a, const NvDsInferObjectDetectionInfo& b) {
    const float ax2 = a.left + a.width;
    const float ay2 = a.top + a.height;
    const float bx2 = b.left + b.width;
    const float by2 = b.top + b.height;

    const float ix1 = std::max(a.left, b.left);
    const float iy1 = std::max(a.top, b.top);
    const float ix2 = std::min(ax2, bx2);
    const float iy2 = std::min(ay2, by2);

    const float iw = std::max(0.0F, ix2 - ix1);
    const float ih = std::max(0.0F, iy2 - iy1);
    const float intersection = iw * ih;
    const float union_area = (a.width * a.height) + (b.width * b.height) - intersection;
    return union_area > 0.0F ? intersection / union_area : 0.0F;
}

void nms(std::vector<NvDsInferObjectDetectionInfo>& boxes) {
    std::sort(boxes.begin(), boxes.end(), [](const auto& left, const auto& right) {
        return left.detectionConfidence > right.detectionConfidence;
    });

    std::vector<NvDsInferObjectDetectionInfo> kept;
    kept.reserve(boxes.size());

    for (const auto& candidate : boxes) {
        const bool overlaps = std::any_of(kept.begin(), kept.end(), [&](const auto& selected) {
            return candidate.classId == selected.classId && iou(candidate, selected) > kNmsThreshold;
        });
        if (!overlaps) {
            kept.push_back(candidate);
        }
    }

    boxes = std::move(kept);
}

}  // namespace

extern "C" bool NvDsInferParseYoloPlate(
    std::vector<NvDsInferLayerInfo> const& output_layers,
    NvDsInferNetworkInfo const& network_info,
    NvDsInferParseDetectionParams const& detection_params,
    std::vector<NvDsInferObjectDetectionInfo>& object_list) {
    if (output_layers.empty() || output_layers[0].buffer == nullptr) {
        std::cerr << "YOLO plate parser: missing output layer" << std::endl;
        return false;
    }

    const NvDsInferLayerInfo& output = output_layers[0];
    const auto& dims = output.inferDims;
    int box_count = 1;
    int value_count = 1;

    if (dims.numDims == 3) {
        value_count = dims.d[1];
        box_count = dims.d[2];
    } else if (dims.numDims == 2) {
        value_count = dims.d[0];
        box_count = dims.d[1];
    } else {
        std::cerr << "YOLO plate parser: unsupported output dimensions: " << dims.numDims << std::endl;
        return false;
    }

    if (value_count != kBoxValues || detection_params.numClassesConfigured < 1) {
        std::cerr << "YOLO plate parser: expected 5 values per box and one configured class" << std::endl;
        return false;
    }

    const auto* data = static_cast<const float*>(output.buffer);
    const float threshold = detection_params.perClassPreclusterThreshold.empty()
                                ? 0.35F
                                : detection_params.perClassPreclusterThreshold[0];

    std::vector<NvDsInferObjectDetectionInfo> parsed;
    parsed.reserve(static_cast<std::size_t>(box_count));

    for (int index = 0; index < box_count; ++index) {
        const float cx = data[index];
        const float cy = data[box_count + index];
        const float width = data[(2 * box_count) + index];
        const float height = data[(3 * box_count) + index];
        const float confidence = data[(4 * box_count) + index];

        if (!std::isfinite(confidence) || confidence < threshold || width <= 0.0F || height <= 0.0F) {
            continue;
        }

        NvDsInferObjectDetectionInfo object{};
        object.classId = 0;
        object.detectionConfidence = confidence;
        object.left = clamp(cx - (width * 0.5F), 0.0F, static_cast<float>(network_info.width - 1));
        object.top = clamp(cy - (height * 0.5F), 0.0F, static_cast<float>(network_info.height - 1));
        const float right = clamp(cx + (width * 0.5F), 0.0F, static_cast<float>(network_info.width - 1));
        const float bottom = clamp(cy + (height * 0.5F), 0.0F, static_cast<float>(network_info.height - 1));
        object.width = std::max(0.0F, right - object.left);
        object.height = std::max(0.0F, bottom - object.top);

        if (object.width > 1.0F && object.height > 1.0F) {
            parsed.push_back(object);
        }
    }

    nms(parsed);
    object_list.insert(object_list.end(), parsed.begin(), parsed.end());
    return true;
}

CHECK_CUSTOM_PARSE_FUNC_PROTOTYPE(NvDsInferParseYoloPlate);
