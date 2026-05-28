#pragma once

#include "anpr_types.hpp"

#include <optional>
#include <string>

namespace anpr {

class PlatePostProcessor {
public:
    std::string normalize(const std::string& raw_text) const;
    bool isValidIndianPlate(const std::string& plate_text) const;
    std::optional<OcrResult> accept(const OcrResult& result) const;
};

}  // namespace anpr
