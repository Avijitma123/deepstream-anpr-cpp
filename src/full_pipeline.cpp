#include "full_pipeline.hpp"

#include <gst/gst.h>

#include "gstnvdsmeta.h"
#include "nvbufsurface.h"
#include "nvdsmeta.h"
#include "nvds_obj_encode.h"

#include <chrono>
#include <array>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace anpr {
namespace {

struct ProbeContext {
    FullPipelineConfig config;
    PlatePostProcessor* post_processor{nullptr};
    EventManager* event_manager{nullptr};
    NvDsObjEncCtxHandle encoder{nullptr};
    std::uint64_t crop_index{0};
    std::size_t ocr_attempts{0};
    std::unordered_set<std::uint64_t> processed_tracking_ids;
};

std::string timestampForFile() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time, &tm);

    std::ostringstream output;
    output << std::put_time(&tm, "%Y%m%dT%H%M%SZ");
    return output.str();
}

std::string shellQuote(const std::string& value) {
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

OcrResult runOcrCommand(const std::filesystem::path& ocr_binary, const std::filesystem::path& crop_path) {
    const auto command = shellQuote(ocr_binary.string()) + " --image " + shellQuote(crop_path.string()) + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return {};
    }

    std::array<char, 256> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    const int status = pclose(pipe);
    if (status != 0 || output.empty()) {
        return {};
    }

    std::istringstream stream(output);
    OcrResult result;
    stream >> result.text;
    std::string confidence_token;
    stream >> confidence_token;
    const auto separator = confidence_token.find('=');
    if (separator != std::string::npos) {
        result.confidence = std::stof(confidence_token.substr(separator + 1));
    }
    return result;
}

gboolean busCall(GstBus*, GstMessage* message, gpointer data) {
    auto* loop = static_cast<GMainLoop*>(data);

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_EOS:
            std::cout << "EOS received - stopping full pipeline\n";
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            GError* error = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            std::cerr << "GStreamer error: " << (error != nullptr ? error->message : "unknown") << '\n';
            if (debug != nullptr) {
                std::cerr << "Debug info: " << debug << '\n';
            }
            g_clear_error(&error);
            g_free(debug);
            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }

    return TRUE;
}

GstPadProbeReturn metadataProbe(GstPad*, GstPadProbeInfo* info, gpointer user_data) {
    auto* context = static_cast<ProbeContext*>(user_data);
    auto* buffer = static_cast<GstBuffer*>(info->data);
    if (buffer == nullptr) {
        return GST_PAD_PROBE_OK;
    }

    GstMapInfo map_info{};
    if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ)) {
        return GST_PAD_PROBE_OK;
    }

    auto* surface = reinterpret_cast<NvBufSurface*>(map_info.data);
    gst_buffer_unmap(buffer, &map_info);

    NvDsBatchMeta* batch_meta = gst_buffer_get_nvds_batch_meta(buffer);
    if (batch_meta == nullptr || surface == nullptr) {
        return GST_PAD_PROBE_OK;
    }

    for (NvDsMetaList* frame_node = batch_meta->frame_meta_list; frame_node != nullptr; frame_node = frame_node->next) {
        auto* frame_meta = static_cast<NvDsFrameMeta*>(frame_node->data);
        if (frame_meta == nullptr) {
            continue;
        }

        for (NvDsMetaList* object_node = frame_meta->obj_meta_list; object_node != nullptr;
             object_node = object_node->next) {
            auto* object_meta = static_cast<NvDsObjectMeta*>(object_node->data);
            if (object_meta == nullptr || object_meta->class_id != 0) {
                continue;
            }

            const auto tracking_id = static_cast<std::uint64_t>(object_meta->object_id);
            if (context->ocr_attempts >= context->config.max_ocr_attempts ||
                context->processed_tracking_ids.find(tracking_id) != context->processed_tracking_ids.end()) {
                continue;
            }
            context->processed_tracking_ids.insert(tracking_id);
            ++context->ocr_attempts;

            std::filesystem::create_directories(context->config.evidence_dir);
            const auto crop_path = context->config.evidence_dir /
                                   (context->config.camera_id + "_" + std::to_string(frame_meta->frame_num) + "_" +
                                    std::to_string(context->crop_index++) + "_" + timestampForFile() + ".jpg");

            NvDsObjEncUsrArgs encode_args{};
            encode_args.saveImg = context->config.save_crops;
            encode_args.attachUsrMeta = false;
            encode_args.scaleImg = true;
            encode_args.scaledWidth = 96;
            encode_args.scaledHeight = 48;
            encode_args.objNum = static_cast<int>(context->crop_index);
            encode_args.quality = 95;
            encode_args.isFrame = false;
            std::strncpy(encode_args.fileNameImg, crop_path.string().c_str(), FILE_NAME_SIZE - 1);

            if (!nvds_obj_enc_process(context->encoder, &encode_args, surface, object_meta, frame_meta)) {
                std::cerr << "Failed to encode plate crop: " << crop_path << '\n';
                continue;
            }
            nvds_obj_enc_finish(context->encoder);

            auto ocr_result = runOcrCommand(context->config.ocr_binary, crop_path);
            auto accepted = context->post_processor->accept(ocr_result);
            if (!accepted.has_value()) {
                ocr_result.text = context->post_processor->normalize(ocr_result.text);
                if (ocr_result.text.size() < 4 || ocr_result.confidence <= 0.0F) {
                    continue;
                }
                accepted = ocr_result;
            }

            if (!accepted.has_value()) {
                continue;
            }

            PlateEvent event;
            event.camera_id = context->config.camera_id;
            event.tracking_id = object_meta->object_id;
            event.plate_text = accepted->text;
            event.confidence = accepted->confidence;
            event.timestamp = std::chrono::system_clock::now();
            event.bbox.left = object_meta->rect_params.left;
            event.bbox.top = object_meta->rect_params.top;
            event.bbox.width = object_meta->rect_params.width;
            event.bbox.height = object_meta->rect_params.height;
            event.evidence_path = crop_path.string();

            if (context->event_manager->submit(event)) {
                std::cout << "ANPR event: " << event.plate_text << " confidence=" << event.confidence
                          << " crop=" << event.evidence_path << '\n';
            }
        }
    }

    return GST_PAD_PROBE_OK;
}

}  // namespace

FullPipeline::FullPipeline(FullPipelineConfig config,
                           PlatePostProcessor& post_processor,
                           EventManager& event_manager)
    : config_(std::move(config)),
      post_processor_(post_processor),
      event_manager_(event_manager) {}

int FullPipeline::run() {
    setenv("GST_PLUGIN_PATH", "/opt/nvidia/deepstream/deepstream-9.0/lib/gst-plugins", 0);

    gst_init(nullptr, nullptr);

    PipelineBuilder builder(config_.pipeline);
    auto pipeline_description = builder.buildLaunchPipeline();
    pipeline_description.erase(std::remove(pipeline_description.begin(), pipeline_description.end(), '\''),
                               pipeline_description.end());
    std::cout << "Full DeepStream pipeline:\n" << pipeline_description << '\n';

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipeline_description.c_str(), &error);
    if (pipeline == nullptr) {
        std::cerr << "Failed to create pipeline: " << (error != nullptr ? error->message : "unknown") << '\n';
        g_clear_error(&error);
        return 1;
    }

    GstElement* tracker = gst_bin_get_by_name(GST_BIN(pipeline), "tracker");
    if (tracker == nullptr) {
        std::cerr << "Failed to find tracker element for metadata probe\n";
        gst_object_unref(pipeline);
        return 1;
    }

    GstPad* probe_pad = gst_element_get_static_pad(tracker, "src");
    if (probe_pad == nullptr) {
        std::cerr << "Failed to get tracker src pad\n";
        gst_object_unref(tracker);
        gst_object_unref(pipeline);
        return 1;
    }

    ProbeContext probe_context;
    probe_context.config = config_;
    probe_context.post_processor = &post_processor_;
    probe_context.event_manager = &event_manager_;
    probe_context.encoder = nvds_obj_enc_create_context(0);
    if (probe_context.encoder == nullptr) {
        std::cerr << "Failed to create DeepStream object encoder context\n";
        gst_object_unref(probe_pad);
        gst_object_unref(tracker);
        gst_object_unref(pipeline);
        return 1;
    }

    gst_pad_add_probe(probe_pad, GST_PAD_PROBE_TYPE_BUFFER, metadataProbe, &probe_context, nullptr);
    gst_object_unref(probe_pad);
    gst_object_unref(tracker);

    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_watch(bus, busCall, loop);
    gst_object_unref(bus);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    nvds_obj_enc_finish(probe_context.encoder);
    nvds_obj_enc_destroy_context(probe_context.encoder);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    std::cout << "Accepted events: " << event_manager_.acceptedCount()
              << ", suppressed events: " << event_manager_.suppressedCount() << '\n';
    return 0;
}

}  // namespace anpr
