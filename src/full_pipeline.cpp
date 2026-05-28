#include "full_pipeline.hpp"

#include <gst/gst.h>

#include "gstnvdsmeta.h"
#include "nvbufsurface.h"
#include "nvdsmeta.h"
#include "nvds_obj_encode.h"
#include "plate_cropper.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace anpr {
namespace {

struct ProbeContext {
    FullPipelineConfig config;
    PlatePostProcessor* post_processor{nullptr};
    EventManager* event_manager{nullptr};
    PlateCropper* cropper{nullptr};
    NvDsObjEncCtxHandle encoder{nullptr};
    class OcrWorker* ocr_worker{nullptr};
    std::size_t ocr_attempts{0};
    std::unordered_map<std::uint64_t, std::size_t> ocr_attempts_by_tracking_id;
    std::unordered_set<std::uint64_t> completed_tracking_ids;
};

class OcrWorker {
public:
    explicit OcrWorker(std::filesystem::path binary) : binary_(std::move(binary)) {}

    ~OcrWorker() {
        stop(false);
    }

    bool start() {
        int stdin_pipe[2]{};
        int stdout_pipe[2]{};
        if (pipe(stdin_pipe) != 0) {
            return false;
        }
        if (pipe(stdout_pipe) != 0) {
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            return false;
        }

        child_pid_ = fork();
        if (child_pid_ < 0) {
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            return false;
        }

        if (child_pid_ == 0) {
            dup2(stdin_pipe[0], STDIN_FILENO);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            const int dev_null = open("/dev/null", O_WRONLY);
            if (dev_null >= 0) {
                dup2(dev_null, STDERR_FILENO);
                close(dev_null);
            }
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            execl(binary_.c_str(), binary_.c_str(), "--server", static_cast<char*>(nullptr));
            _exit(127);
        }

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        input_ = fdopen(stdin_pipe[1], "w");
        output_ = fdopen(stdout_pipe[0], "r");
        if (input_ == nullptr || output_ == nullptr) {
            if (input_ == nullptr) {
                close(stdin_pipe[1]);
            }
            if (output_ == nullptr) {
                close(stdout_pipe[0]);
            }
            stop(true);
            return false;
        }
        return true;
    }

    OcrResult recognize(const std::filesystem::path& crop_path, int timeout_ms) {
        if (input_ == nullptr || output_ == nullptr) {
            return {};
        }

        if (std::fprintf(input_, "%s\n", crop_path.string().c_str()) < 0 || std::fflush(input_) != 0) {
            std::cerr << "OCR worker write failed; restarting worker\n";
            restart();
            return {};
        }

        if (!waitForOutput(timeout_ms)) {
            std::cerr << "OCR worker timed out on crop: " << crop_path << "; restarting worker\n";
            restart();
            return {};
        }

        std::array<char, 512> buffer{};
        if (std::fgets(buffer.data(), static_cast<int>(buffer.size()), output_) == nullptr) {
            std::cerr << "OCR worker stopped unexpectedly; restarting worker\n";
            restart();
            return {};
        }

        std::istringstream stream(buffer.data());
        std::string status;
        stream >> status;
        if (status != "OK") {
            return {};
        }

        OcrResult result;
        stream >> result.text >> result.confidence;
        return result;
    }

private:
    bool waitForOutput(int timeout_ms) const {
        if (output_ == nullptr) {
            return false;
        }

        const int output_fd = fileno(output_);
        if (output_fd < 0) {
            return false;
        }

        pollfd descriptor{};
        descriptor.fd = output_fd;
        descriptor.events = POLLIN;

        int result = 0;
        do {
            result = poll(&descriptor, 1, timeout_ms);
        } while (result < 0 && errno == EINTR);

        return result > 0 && (descriptor.revents & POLLIN) != 0;
    }

    void restart() {
        stop(true);
        if (!start()) {
            std::cerr << "Failed to restart OCR worker: " << binary_ << '\n';
        }
    }

    void stop(bool force) {
        if (input_ != nullptr) {
            if (!force) {
                std::fprintf(input_, "__quit__\n");
                std::fflush(input_);
            }
            std::fclose(input_);
            input_ = nullptr;
        }
        if (output_ != nullptr) {
            std::fclose(output_);
            output_ = nullptr;
        }
        if (child_pid_ > 0) {
            if (force) {
                kill(child_pid_, SIGTERM);
            }
            int status = 0;
            waitpid(child_pid_, &status, 0);
            child_pid_ = -1;
        }
    }

    std::filesystem::path binary_;
    pid_t child_pid_{-1};
    FILE* input_{nullptr};
    FILE* output_{nullptr};
};

void applyCropPadding(NvDsObjectMeta* object_meta, const FullPipelineConfig& config, guint frame_width, guint frame_height) {
    if (frame_width == 0 || frame_height == 0 || config.crop_padding_ratio <= 0.0F) {
        return;
    }

    auto& rect = object_meta->rect_params;
    const float pad_x = rect.width * config.crop_padding_ratio;
    const float pad_y = rect.height * config.crop_padding_ratio;
    const float left = std::max(0.0F, rect.left - pad_x);
    const float top = std::max(0.0F, rect.top - pad_y);
    const float right = std::min(static_cast<float>(frame_width - 1), rect.left + rect.width + pad_x);
    const float bottom = std::min(static_cast<float>(frame_height - 1), rect.top + rect.height + pad_y);

    rect.left = left;
    rect.top = top;
    rect.width = std::max(1.0F, right - left);
    rect.height = std::max(1.0F, bottom - top);
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

            if (object_meta->confidence < context->config.min_detector_confidence ||
                object_meta->rect_params.width < context->config.min_crop_width ||
                object_meta->rect_params.height < context->config.min_crop_height) {
                continue;
            }

            const auto tracking_id = static_cast<std::uint64_t>(object_meta->object_id);
            if ((context->config.max_ocr_attempts > 0 && context->ocr_attempts >= context->config.max_ocr_attempts) ||
                context->completed_tracking_ids.find(tracking_id) != context->completed_tracking_ids.end()) {
                continue;
            }

            const auto track_attempts = context->ocr_attempts_by_tracking_id.find(tracking_id);
            if (context->config.max_ocr_attempts_per_track > 0 &&
                track_attempts != context->ocr_attempts_by_tracking_id.end() &&
                track_attempts->second >= context->config.max_ocr_attempts_per_track) {
                continue;
            }
            ++context->ocr_attempts_by_tracking_id[tracking_id];
            ++context->ocr_attempts;

            const auto event_time = std::chrono::system_clock::now();
            PlateDetection detection;
            detection.camera_id = context->config.camera_id;
            detection.tracking_id = tracking_id;
            detection.bbox.left = object_meta->rect_params.left;
            detection.bbox.top = object_meta->rect_params.top;
            detection.bbox.width = object_meta->rect_params.width;
            detection.bbox.height = object_meta->rect_params.height;
            detection.detector_confidence = object_meta->confidence;
            detection.timestamp = event_time;
            const auto crop_path = context->cropper->crop(detection);

            NvDsObjEncUsrArgs encode_args{};
            encode_args.saveImg = context->config.save_crops;
            encode_args.attachUsrMeta = false;
            encode_args.scaleImg = true;
            encode_args.scaledWidth = 96;
            encode_args.scaledHeight = 48;
            encode_args.objNum = static_cast<int>(tracking_id);
            encode_args.quality = 95;
            encode_args.isFrame = false;
            std::strncpy(encode_args.fileNameImg, crop_path.string().c_str(), FILE_NAME_SIZE - 1);

            const auto original_rect = object_meta->rect_params;
            applyCropPadding(object_meta, context->config, frame_meta->source_frame_width, frame_meta->source_frame_height);
            if (!nvds_obj_enc_process(context->encoder, &encode_args, surface, object_meta, frame_meta)) {
                object_meta->rect_params = original_rect;
                std::cerr << "Failed to encode plate crop: " << crop_path << '\n';
                continue;
            }
            nvds_obj_enc_finish(context->encoder);
            object_meta->rect_params = original_rect;

            auto ocr_result = context->ocr_worker->recognize(crop_path, context->config.ocr_timeout_ms);
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

            context->completed_tracking_ids.insert(tracking_id);

            PlateEvent event;
            event.camera_id = context->config.camera_id;
            event.tracking_id = object_meta->object_id;
            event.plate_text = accepted->text;
            event.confidence = accepted->confidence;
            event.timestamp = event_time;
            event.bbox = detection.bbox;
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
    PlateCropper cropper(config_.evidence_dir);
    if (!cropper.prepare()) {
        std::cerr << "Failed to prepare evidence directory: " << config_.evidence_dir << '\n';
        gst_object_unref(probe_pad);
        gst_object_unref(tracker);
        gst_object_unref(pipeline);
        return 1;
    }
    probe_context.cropper = &cropper;
    OcrWorker ocr_worker(config_.ocr_binary);
    if (!ocr_worker.start()) {
        std::cerr << "Failed to start OCR worker: " << config_.ocr_binary << '\n';
        gst_object_unref(probe_pad);
        gst_object_unref(tracker);
        gst_object_unref(pipeline);
        return 1;
    }
    probe_context.ocr_worker = &ocr_worker;
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
