#include "ocr_engine.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage(const char* executable) {
    std::cout << "Usage: " << executable << " --image <plate-crop> [--ocr-config <path>]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path image_path;
    std::filesystem::path config_path{"configs/config_ocr.txt"};

    try {
        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            const auto requireValue = [&](const std::string& name) -> std::string {
                if (index + 1 >= argc) {
                    throw std::runtime_error("missing value for " + name);
                }
                return argv[++index];
            };

            if (arg == "--image") {
                image_path = requireValue(arg);
            } else if (arg == "--ocr-config") {
                config_path = requireValue(arg);
            } else if (arg == "--help") {
                printUsage(argv[0]);
                return 0;
            } else {
                throw std::runtime_error("unknown argument: " + arg);
            }
        }

        if (image_path.empty()) {
            printUsage(argv[0]);
            return 1;
        }

        anpr::OcrEngine ocr({config_path});
        if (!ocr.load()) {
            std::cerr << "Failed to load OCR config: " << config_path << '\n';
            return 1;
        }

        const auto result = ocr.recognize({}, image_path);
        if (result.text.empty()) {
            return 2;
        }

        std::cout << result.text << " confidence=" << result.confidence << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
