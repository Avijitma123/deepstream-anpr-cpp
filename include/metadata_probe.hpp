#pragma once

#include "anpr_types.hpp"
#include "event_manager.hpp"
#include "ocr_engine.hpp"
#include "plate_cropper.hpp"
#include "plate_postprocess.hpp"

#include <vector>

namespace anpr {

class MetadataProbe {
public:
    MetadataProbe(PlateCropper& cropper,
                  OcrEngine& ocr,
                  PlatePostProcessor& post_processor,
                  EventManager& event_manager);

    void processDetections(const std::vector<PlateDetection>& detections);

private:
    PlateCropper& cropper_;
    OcrEngine& ocr_;
    PlatePostProcessor& post_processor_;
    EventManager& event_manager_;
};

}  // namespace anpr
