#pragma once

#include <cstdint>
#include <liburing.h>

#include "order_book.hpp"
#include "messages.hpp"

namespace engine {

class FeedHandler {
public:
    FeedHandler(OrderBook& book, uint16_t port);
    ~FeedHandler();

    FeedHandler(const FeedHandler&) = delete;
    FeedHandler& operator=(const FeedHandler&) = delete;

    void run_for(int max_messages);
private:
    OrderBook& book_;
    int socket_fd_ = -1;
    io_uring ring_{};

    void setup_socket(uint16_t port);
    void submit_read(uint8_t* buffer, size_t buffer_len);
    void process_message(const uint8_t* buffer, size_t len);
};
}  //namespace engine