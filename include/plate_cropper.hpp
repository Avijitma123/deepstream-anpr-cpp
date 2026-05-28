#pragma once

#include "anpr_types.hpp"

#include <filesystem>

namespace anpr {

class PlateCropper {
public:
    explicit PlateCropper(std::filesystem::path evidence_dir);

    bool prepare();
    std::filesystem::path crop(const PlateDetection& detection);

private:
    std::filesystem::path evidence_dir_;
};

}  // namespace anpr
