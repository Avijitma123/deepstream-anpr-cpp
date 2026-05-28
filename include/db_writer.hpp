#pragma once

#include "anpr_types.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace anpr {

class DbWriter {
public:
    explicit DbWriter(std::filesystem::path csv_path);

    bool open();
    bool writeEvent(const PlateEvent& event);
    const std::filesystem::path& path() const;

private:
    std::filesystem::path csv_path_;
    std::ofstream stream_;
};

}  // namespace anpr
