#pragma once

#include <filesystem>
#include <string>

namespace anpr {

struct PipelineConfig {
    std::string source_uri;
    std::filesystem::path detector_config{"configs/config_infer_plate_detector.txt"};
    std::filesystem::path tracker_config{"/opt/nvidia/deepstream/deepstream-9.0/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml"};
    std::filesystem::path tracker_lib{"/opt/nvidia/deepstream/deepstream-9.0/lib/libnvds_nvmultiobjecttracker.so"};
    int width{1280};
    int height{720};
    int batch_size{1};
    bool display{true};
    bool sync{false};
};

class PipelineBuilder {
public:
    explicit PipelineBuilder(PipelineConfig config);

    std::string buildLaunchPipeline() const;
    int run() const;

private:
    static std::string quote(const std::string& value);
    static std::string toUri(const std::string& source);

    PipelineConfig config_;
};

}  // namespace anpr
