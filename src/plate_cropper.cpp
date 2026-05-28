#include "plate_cropper.hpp"

#include <cctype>
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

std::string safePathToken(const std::string& value) {
    std::string safe;
    safe.reserve(value.size());
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) != 0 || ch == '-' || ch == '_') {
            safe.push_back(static_cast<char>(ch));
        } else {
            safe.push_back('_');
        }
    }
    return safe.empty() ? "camera" : safe;
}

}  // namespace

PlateCropper::PlateCropper(std::filesystem::path evidence_dir) : evidence_dir_(std::move(evidence_dir)) {}

bool PlateCropper::prepare() {
    std::filesystem::create_directories(evidence_dir_);
    return std::filesystem::exists(evidence_dir_);
}

std::filesystem::path PlateCropper::crop(const PlateDetection& detection) {
    std::ostringstream file_name;
    file_name << safePathToken(detection.camera_id) << '_'
              << detection.tracking_id << '_'
              << crop_index_++ << '_'
              << timestampForPath(detection.timestamp) << ".jpg";
    return evidence_dir_ / file_name.str();
}

}  // namespace anpr
