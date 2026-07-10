#pragma once

#include <cstdint>
#include <liburing.h>

#include "spsc_queue.hpp"   // was: order_book.hpp
#include "messages.hpp"

namespace engine {

class FeedHandler {
public:
    FeedHandler(SpscQueue<FeedMessage, 1024>& queue, uint16_t port);  
    ~FeedHandler();

    FeedHandler(const FeedHandler&) = delete;
    FeedHandler& operator=(const FeedHandler&) = delete;

    void run_for(int max_messages);

    uint64_t gaps_detected() const { return gaps_detected_; }
    uint64_t duplicates_dropped() const { return duplicates_dropped_; }
private:
    SpscQueue<FeedMessage, 1024>& queue_;   
    int socket_fd_ = -1;
    io_uring ring_{};

    uint64_t expected_sequence_ = 0;
    uint64_t gaps_detected_ = 0;
    uint64_t duplicates_dropped_ = 0;

    bool check_sequence(uint64_t sequence);

    void setup_socket(uint16_t port);
    void submit_read(uint8_t* buffer, size_t buffer_len);
    void process_message(const uint8_t* buffer, size_t len);
};
}  // namespace engine