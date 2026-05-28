#include "event_manager.hpp"

#include <utility>

namespace anpr {

EventManager::EventManager(EventManagerConfig config, DbWriter writer)
    : config_(config), writer_(std::move(writer)) {}

bool EventManager::start() {
    return writer_.open();
}

bool EventManager::submit(const PlateEvent& event) {
    if (event.plate_text.empty() || event.confidence < config_.min_confidence || isDuplicate(event)) {
        ++suppressed_count_;
        return false;
    }

    if (!writer_.writeEvent(event)) {
        return false;
    }

    remember(event);
    ++accepted_count_;
    return true;
}

std::size_t EventManager::acceptedCount() const {
    return accepted_count_;
}

std::size_t EventManager::suppressedCount() const {
    return suppressed_count_;
}

bool EventManager::isDuplicate(const PlateEvent& event) {
    const auto key = event.camera_id + ":" + event.plate_text;
    const auto existing = last_seen_.find(key);
    if (existing == last_seen_.end()) {
        return false;
    }
    return event.timestamp - existing->second < config_.duplicate_window;
}

void EventManager::remember(const PlateEvent& event) {
    const auto key = event.camera_id + ":" + event.plate_text;
    last_seen_[key] = event.timestamp;
}

}  // namespace anpr
