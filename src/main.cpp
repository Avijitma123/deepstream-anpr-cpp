#include "db_writer.hpp"
#include "event_manager.hpp"
#include "full_pipeline.hpp"
#include "pipeline_builder.hpp"
#include "plate_postprocess.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct CliOptions {
    std::string source;
    std::string camera_id{"camera-01"};
    std::filesystem::path detector_config{"configs/config_infer_plate_detector.txt"};
    std::filesystem::path tracker_config{"/opt/nvidia/deepstream/deepstream-9.0/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml"};
    std::filesystem::path evidence_dir{"evidence"};
    std::filesystem::path events_csv{"output/events.csv"};
    bool run_pipeline{false};
    bool display{true};
};

void printUsage(const char* executable) {
    std::cout
        << "Usage: " << executable << " --source <rtsp-url|video-file> [options]\n\n"
        << "Options:\n"
        << "  --camera-id <id>             Camera identifier for generated events\n"
        << "  --detector-config <path>     DeepStream nvinfer config path\n"
        << "  --tracker-config <path>      DeepStream tracker config path\n"
        << "  --evidence-dir <path>        Plate crop output directory\n"
        << "  --events <path>              CSV event log path\n"
        << "  --no-display                 Use fakesink instead of rendering output\n"
        << "  --run                        Launch the generated GStreamer pipeline\n"
        << "  --help                       Show this help\n";
}

bool parseArgs(int argc, char** argv, CliOptions& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        const auto requireValue = [&](const std::string& name) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error("missing value for " + name);
            }
            return argv[++index];
        };

        if (arg == "--source") {
            options.source = requireValue(arg);
        } else if (arg == "--camera-id") {
            options.camera_id = requireValue(arg);
        } else if (arg == "--detector-config") {
            options.detector_config = requireValue(arg);
        } else if (arg == "--tracker-config") {
            options.tracker_config = requireValue(arg);
        } else if (arg == "--evidence-dir") {
            options.evidence_dir = requireValue(arg);
        } else if (arg == "--events") {
            options.events_csv = requireValue(arg);
        } else if (arg == "--no-display") {
            options.display = false;
        } else if (arg == "--run") {
            options.run_pipeline = true;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return false;
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if (options.source.empty()) {
        printUsage(argv[0]);
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    CliOptions options;

    try {
        if (!parseArgs(argc, argv, options)) {
            return options.source.empty() ? 1 : 0;
        }

        anpr::EventManager event_manager({}, anpr::DbWriter(options.events_csv));
        if (!event_manager.start()) {
            std::cerr << "Failed to open event log: " << options.events_csv << '\n';
            return 1;
        }

        anpr::PlatePostProcessor post_processor;

        anpr::PipelineConfig pipeline_config;
        pipeline_config.source_uri = options.source;
        pipeline_config.detector_config = options.detector_config;
        pipeline_config.tracker_config = options.tracker_config;
        pipeline_config.display = options.display;

        anpr::PipelineBuilder builder(pipeline_config);
        std::cout << "DeepStream pipeline:\n" << builder.buildLaunchPipeline() << "\n";

        if (options.run_pipeline) {
            anpr::FullPipelineConfig full_config;
            full_config.pipeline = pipeline_config;
            full_config.camera_id = options.camera_id;
            full_config.evidence_dir = options.evidence_dir;
            anpr::FullPipeline full_pipeline(full_config, post_processor, event_manager);
            return full_pipeline.run();
        }

        std::cout << "\nRun again with --run to launch it on a DeepStream-enabled host.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
