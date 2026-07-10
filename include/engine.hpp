#pragma once

#include <cstdint>

#include "spsc_queue.hpp"
#include "messages.hpp"
#include "order_book.hpp"
#include "feed_handler.hpp"
#include "matching_thread.hpp"
#include "latency_recorder.hpp"

namespace engine{

class Engine{
public:
    Engine(uint16_t port, int32_t reference_price_ticks, int32_t window_ticks,size_t expected_message_count = 100000 );

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // engine.hpp
    void start(int cpu_core = -1);   // change signature
    void stop();

    const OrderBook& book() const {return book_;}
    const LatencyRecorder& recorder() const { return recorder_; } 

private:
    SpscQueue<FeedMessage, 1024> queue_;
    OrderBook book_;
    LatencyRecorder recorder_;
    FeedHandler feed_handler_;
    MatchingThread matcher_;
};

} //namespace engine