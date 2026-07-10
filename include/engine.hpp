#pragma once

#include <cstdint>

#include "spsc_queue.hpp"
#include "messages.hpp"
#include "order_book.hpp"
#include "feed_handler.hpp"
#include "matching_thread.hpp"

namespace engine{

class Engine{
public:
    Engine(uint16_t port, int32_t reference_price_ticks, int32_t window_ticks);

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void start();
    void stop();

    const OrderBook& book() const {return book_;}

private:
    SpscQueue<FeedMessage, 1024> queue_;
    OrderBook book_;
    FeedHandler feed_handler_;
    MatchingThread matcher_;
};

} //namespace engine