#pragma once

#include "anpr_types.hpp"

#include <cstdint>
#include <filesystem>

namespace anpr {

class PlateCropper {
public:
    explicit PlateCropper(std::filesystem::path evidence_dir);

    bool prepare();
    std::filesystem::path crop(const PlateDetection& detection);

private:
    std::filesystem::path evidence_dir_;
    std::uint64_t crop_index_{0};
};

}  // namespace anpr
