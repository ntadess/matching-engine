// include/matching_thread.hpp
#pragma once

#include <atomic>
#include <thread>
#include <variant>

#include "spsc_queue.hpp"
#include "messages.hpp"
#include "order_book.hpp"
#include "latency_recorder.hpp"
#include <pthread.h>

namespace engine {

class MatchingThread {
public:
    MatchingThread(SpscQueue<FeedMessage, 1024>& queue, OrderBook& book, LatencyRecorder* recorder = nullptr);

    // Not copyable or movable 
    MatchingThread(const MatchingThread&) = delete;
    MatchingThread& operator=(const MatchingThread&) = delete;

    void start(int cpu_core = -1);   // launches the consumer thread
    void stop();    // joins the thread
    ~MatchingThread();

private:
    void run();     

    SpscQueue<FeedMessage, 1024>& queue_;
    OrderBook& book_;
    LatencyRecorder* recorder_ = nullptr;
    std::thread matcher_thread_;
    std::atomic<bool> running_{false};
};

}  // namespace engine