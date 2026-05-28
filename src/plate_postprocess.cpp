#include "plate_postprocess.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace anpr {

std::string PlatePostProcessor::normalize(const std::string& raw_text) const {
    std::string normalized;
    normalized.reserve(raw_text.size());

    for (const unsigned char ch : raw_text) {
        if (std::isalnum(ch) != 0) {
            normalized.push_back(static_cast<char>(std::toupper(ch)));
        }
    }

    return normalized;
}

bool PlatePostProcessor::isValidIndianPlate(const std::string& plate_text) const {
    static const std::regex standard_pattern("^[A-Z]{2}[0-9]{1,2}[A-Z]{1,3}[0-9]{4}$");
    static const std::regex bharat_pattern("^BH[0-9]{2}[A-Z]{2}[0-9]{4}$");
    return std::regex_match(plate_text, standard_pattern) || std::regex_match(plate_text, bharat_pattern);
}

std::optional<OcrResult> PlatePostProcessor::accept(const OcrResult& result) const {
    OcrResult normalized;
    normalized.text = normalize(result.text);
    normalized.confidence = result.confidence;

    if (!isValidIndianPlate(normalized.text)) {
        return std::nullopt;
    }
    return normalized;
}

}  // namespace anpr
