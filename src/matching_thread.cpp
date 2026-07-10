// src/matching_thread.cpp
#include "matching_thread.hpp"
#include <type_traits>

namespace engine {

MatchingThread::MatchingThread(SpscQueue<FeedMessage, 1024>& queue, OrderBook& book,
                                LatencyRecorder* recorder)
    : queue_(queue), book_(book), recorder_(recorder) {}

void MatchingThread::start() {
    running_.store(true, std::memory_order_relaxed);
    matcher_thread_ = std::thread(&MatchingThread::run, this);
}

void MatchingThread::stop() {
    running_.store(false, std::memory_order_relaxed);
    if (matcher_thread_.joinable()) {
        matcher_thread_.join();
    }
}

MatchingThread::~MatchingThread() {
    stop();  
}

void MatchingThread::run() {
    while (running_.load(std::memory_order_relaxed)) {
        auto msg = queue_.pop();
        if (!msg.has_value()) {
            continue;
        }

        std::visit([this](auto&& m) {
            using T = std::decay_t<decltype(m)>;

            if (recorder_) recorder_->record_popped(m.sequence);   // add

            if constexpr (std::is_same_v<T, AddMessage>) {
                book_.add_order(m.id, m.side, m.price_ticks, m.quantity);
            } else if constexpr (std::is_same_v<T, CancelMessage>) {
                book_.cancel_order(m.id);
            }

            if (recorder_) recorder_->record_applied(m.sequence);   // add
        }, *msg);
    }
}

}  // namespace engine