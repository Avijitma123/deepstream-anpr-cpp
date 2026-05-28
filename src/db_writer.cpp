#include "db_writer.hpp"

#include <iomanip>
#include <sstream>

namespace anpr {
namespace {

std::string formatTime(const std::chrono::system_clock::time_point& value) {
    const auto time = std::chrono::system_clock::to_time_t(value);
    std::tm tm{};
    gmtime_r(&time, &tm);

    std::ostringstream output;
    output << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

}  // namespace

DbWriter::DbWriter(std::filesystem::path csv_path) : csv_path_(std::move(csv_path)) {}

bool DbWriter::open() {
    if (!csv_path_.parent_path().empty()) {
        std::filesystem::create_directories(csv_path_.parent_path());
    }

    const bool exists = std::filesystem::exists(csv_path_);
    stream_.open(csv_path_, std::ios::app);
    if (!stream_) {
        return false;
    }

    if (!exists) {
        stream_ << "timestamp,camera_id,tracking_id,plate_text,confidence,left,top,width,height,evidence_path\n";
    }
    return true;
}

bool DbWriter::writeEvent(const PlateEvent& event) {
    if (!stream_) {
        return false;
    }

    stream_ << formatTime(event.timestamp) << ','
            << event.camera_id << ','
            << event.tracking_id << ','
            << event.plate_text << ','
            << event.confidence << ','
            << event.bbox.left << ','
            << event.bbox.top << ','
            << event.bbox.width << ','
            << event.bbox.height << ','
            << event.evidence_path << '\n';
    stream_.flush();
    return static_cast<bool>(stream_);
}

const std::filesystem::path& DbWriter::path() const {
    return csv_path_;
}

}  // namespace anpr
