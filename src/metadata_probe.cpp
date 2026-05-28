#include "metadata_probe.hpp"

namespace anpr {

MetadataProbe::MetadataProbe(PlateCropper& cropper,
                             OcrEngine& ocr,
                             PlatePostProcessor& post_processor,
                             EventManager& event_manager)
    : cropper_(cropper),
      ocr_(ocr),
      post_processor_(post_processor),
      event_manager_(event_manager) {}

void MetadataProbe::processDetections(const std::vector<PlateDetection>& detections) {
    for (const auto& detection : detections) {
        const auto crop_path = cropper_.crop(detection);
        const auto ocr_result = ocr_.recognize(detection, crop_path);
        const auto accepted = post_processor_.accept(ocr_result);
        if (!accepted.has_value()) {
            continue;
        }

        PlateEvent event;
        event.camera_id = detection.camera_id;
        event.tracking_id = detection.tracking_id;
        event.plate_text = accepted->text;
        event.confidence = accepted->confidence;
        event.timestamp = detection.timestamp;
        event.bbox = detection.bbox;
        event.evidence_path = crop_path.string();
        event_manager_.submit(event);
    }
}

}  // namespace anpr
