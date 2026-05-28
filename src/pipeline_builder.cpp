#include "pipeline_builder.hpp"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <utility>

namespace anpr {
namespace {

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

}  // namespace

PipelineBuilder::PipelineBuilder(PipelineConfig config) : config_(std::move(config)) {}

std::string PipelineBuilder::buildLaunchPipeline() const {
    const auto uri = toUri(config_.source_uri);

    std::ostringstream pipeline;
    pipeline
        << "nvstreammux name=m batch-size=" << config_.batch_size
        << " width=" << config_.width
        << " height=" << config_.height
        << " live-source=" << (startsWith(uri, "rtsp://") ? "1" : "0")
        << " ! nvinfer config-file-path=" << quote(config_.detector_config.string())
        << " ! nvtracker tracker-width=640 tracker-height=384 gpu-id=0"
        << " ll-lib-file=" << quote(config_.tracker_lib.string())
        << " ll-config-file=" << quote(config_.tracker_config.string())
        << " ! nvvideoconvert ! nvdsosd ! ";

    if (config_.display) {
        pipeline << "nveglglessink sync=" << (config_.sync ? "true" : "false");
    } else {
        pipeline << "fakesink sync=false";
    }

    pipeline
        << " nvurisrcbin uri=" << quote(uri) << " name=source ! "
        << "queue ! nvvideoconvert ! " << quote("video/x-raw(memory:NVMM),format=NV12") << " ! m.sink_0";

    return pipeline.str();
}

int PipelineBuilder::run() const {
    const std::string command =
        "GST_PLUGIN_PATH=/opt/nvidia/deepstream/deepstream-9.0/lib/gst-plugins "
        "LD_LIBRARY_PATH=/opt/nvidia/deepstream/deepstream-9.0/lib:/usr/local/cuda/lib64:${LD_LIBRARY_PATH} "
        "/usr/bin/gst-launch-1.0 -e " +
        buildLaunchPipeline();
    return std::system(command.c_str());
}

std::string PipelineBuilder::quote(const std::string& value) {
    std::string quoted = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

std::string PipelineBuilder::toUri(const std::string& source) {
    if (startsWith(source, "rtsp://") || startsWith(source, "file://") || startsWith(source, "http://") ||
        startsWith(source, "https://")) {
        return source;
    }

    const auto absolute = std::filesystem::absolute(source);
    return "file://" + absolute.string();
}

}  // namespace anpr
