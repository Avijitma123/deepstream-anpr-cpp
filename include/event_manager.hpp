#pragma once

#include "anpr_types.hpp"
#include "db_writer.hpp"

#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>

namespace anpr {

struct EventManagerConfig {
    std::chrono::seconds duplicate_window{30};
    float min_confidence{0.65F};
};

class EventManager {
public:
    EventManager(EventManagerConfig config, DbWriter writer);

    bool start();
    bool submit(const PlateEvent& event);
    std::size_t acceptedCount() const;
    std::size_t suppressedCount() const;

private:
    bool isDuplicate(const PlateEvent& event);
    void remember(const PlateEvent& event);

    EventManagerConfig config_;
    DbWriter writer_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_seen_;
    std::size_t accepted_count_{0};
    std::size_t suppressed_count_{0};
};

}  // namespace anpr
