// include/latency_recorder.hpp
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace engine {

struct MessageTimestamps {
    std::chrono::steady_clock::time_point received{};
    std::chrono::steady_clock::time_point decoded{};
    std::chrono::steady_clock::time_point popped{};
    std::chrono::steady_clock::time_point applied{};
};

class LatencyRecorder {
public:
    explicit LatencyRecorder(size_t expected_message_count)
        : timestamps_(expected_message_count) {}

    void record_received(uint64_t sequence) {
        timestamps_[sequence].received = std::chrono::steady_clock::now();
    }
    void record_decoded(uint64_t sequence) {
        timestamps_[sequence].decoded = std::chrono::steady_clock::now();
    }
    void record_popped(uint64_t sequence) {
        timestamps_[sequence].popped = std::chrono::steady_clock::now();
    }
    void record_applied(uint64_t sequence) {
        timestamps_[sequence].applied = std::chrono::steady_clock::now();
    }

    double wire_to_book_percentile(double percentile) const {
        return percentile_of(percentile, [](const MessageTimestamps& t) {
            return t.applied - t.received;
        });
    }

    double decode_percentile(double percentile) const {
        return percentile_of(percentile, [](const MessageTimestamps& t) {
            return t.decoded - t.received;
        });
    }

    double queue_transit_percentile(double percentile) const {
        return percentile_of(percentile, [](const MessageTimestamps& t) {
            return t.popped - t.decoded;
        });
    }

    double match_percentile(double percentile) const {
        return percentile_of(percentile, [](const MessageTimestamps& t) {
            return t.applied - t.popped;
        });
    }

private:
    std::vector<MessageTimestamps> timestamps_;

    static bool is_complete(const MessageTimestamps& t) {
        constexpr std::chrono::steady_clock::time_point kEpoch{};
        return t.received != kEpoch && t.decoded != kEpoch &&
               t.popped != kEpoch && t.applied != kEpoch;
    }

    
    template <typename Extractor>
    double percentile_of(double percentile, Extractor extractor) const {
        std::vector<int64_t> durations_ns;
        durations_ns.reserve(timestamps_.size());

        for (const auto& t : timestamps_) {
            if (!is_complete(t)) {
                continue;  // sequence number never received a full message
            }
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                extractor(t));
            durations_ns.push_back(ns.count());
        }

        if (durations_ns.empty()) {
            return 0.0;
        }

        std::sort(durations_ns.begin(), durations_ns.end());

        size_t index = static_cast<size_t>(percentile * (durations_ns.size() - 1));
        return static_cast<double>(durations_ns[index]);
    }
};

}  // namespace engine