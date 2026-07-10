// include/matching_thread.hpp
#pragma once

#include <atomic>
#include <thread>
#include <variant>

#include "spsc_queue.hpp"
#include "messages.hpp"
#include "order_book.hpp"

namespace engine {

class MatchingThread {
public:
    MatchingThread(SpscQueue<FeedMessage, 1024>& queue, OrderBook& book);

    // Not copyable or movable 
    MatchingThread(const MatchingThread&) = delete;
    MatchingThread& operator=(const MatchingThread&) = delete;

    void start();   // launches the consumer thread
    void stop();    // joins the thread
    ~MatchingThread();

private:
    void run();     

    SpscQueue<FeedMessage, 1024>& queue_;
    OrderBook& book_;
    std::thread matcher_thread_;
    std::atomic<bool> running_{false};
};

}  // namespace engine