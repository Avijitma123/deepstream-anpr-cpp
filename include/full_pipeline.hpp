#pragma once

#include "event_manager.hpp"
#include "pipeline_builder.hpp"
#include "plate_postprocess.hpp"

#include <filesystem>
#include <string>

namespace anpr {

struct FullPipelineConfig {
    PipelineConfig pipeline;
    std::string camera_id{"camera-01"};
    std::filesystem::path evidence_dir{"evidence"};
    std::filesystem::path ocr_binary{"build/deepstream-anpr-ocr"};
    std::size_t max_ocr_attempts{0};
    float crop_padding_ratio{0.20F};
    float min_detector_confidence{0.25F};
    float min_crop_width{24.0F};
    float min_crop_height{8.0F};
    bool save_crops{true};
};

class FullPipeline {
public:
    FullPipeline(FullPipelineConfig config,
                 PlatePostProcessor& post_processor,
                 EventManager& event_manager);

    int run();

private:
    FullPipelineConfig config_;
    PlatePostProcessor& post_processor_;
    EventManager& event_manager_;
};

}  // namespace anpr
