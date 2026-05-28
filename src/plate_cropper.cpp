#include "plate_cropper.hpp"

#include <iomanip>
#include <sstream>

namespace anpr {
namespace {

std::string timestampForPath(const std::chrono::system_clock::time_point& value) {
    const auto time = std::chrono::system_clock::to_time_t(value);
    std::tm tm{};
    gmtime_r(&time, &tm);

    std::ostringstream output;
    output << std::put_time(&tm, "%Y%m%dT%H%M%SZ");
    return output.str();
}

}  // namespace

PlateCropper::PlateCropper(std::filesystem::path evidence_dir) : evidence_dir_(std::move(evidence_dir)) {}

bool PlateCropper::prepare() {
    std::filesystem::create_directories(evidence_dir_);
    return std::filesystem::exists(evidence_dir_);
}

std::filesystem::path PlateCropper::crop(const PlateDetection& detection) {
    std::ostringstream file_name;
    file_name << detection.camera_id << '_'
              << detection.tracking_id << '_'
              << timestampForPath(detection.timestamp) << ".jpg";
    return evidence_dir_ / file_name.str();
}

}  // namespace anpr
