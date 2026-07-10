#include "feed_handler.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace engine {

FeedHandler::FeedHandler(OrderBook& book, uint16_t port) : book_(book) {
    setup_socket(port);

    if (io_uring_queue_init(8, &ring_, 0) < 0) {
        throw std::runtime_error("failed to initialize io_uring");
    }
}

FeedHandler::~FeedHandler() {
    io_uring_queue_exit(&ring_);
    if (socket_fd_ >= 0) {
        close(socket_fd_);
    }
}

void FeedHandler::setup_socket(uint16_t port) {
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        throw std::runtime_error("failed to create socket");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("failed to bind socket");
    }
}

void FeedHandler::submit_read(uint8_t* buffer, size_t buffer_len) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    io_uring_prep_recv(sqe, socket_fd_, buffer, buffer_len, 0);
    io_uring_submit(&ring_);
}

void FeedHandler::process_message(const uint8_t* data, size_t len) {
    if (len == 0) {
        return;
    }

    MessageType type = static_cast<MessageType>(data[0]);

    uint64_t sequence;
    std::memcpy(&sequence, data + 1, sizeof(sequence));

    if (!check_sequence(sequence)) {
        return;
    }

    if (type == MessageType::Add && len >= ADD_MESSAGE_SIZE) {
        AddMessage msg = decode_add_message(data);
        book_.add_order(msg.id, msg.side, msg.price_ticks, msg.quantity);
    } else if (type == MessageType::Cancel && len >= CANCEL_MESSAGE_SIZE) {
        CancelMessage msg = decode_cancel_message(data);
        book_.cancel_order(msg.id);
    }
}

void FeedHandler::run_for(int max_messages) {
    uint8_t buffer[ADD_MESSAGE_SIZE];

    for (int i = 0; i < max_messages; ++i) {
        submit_read(buffer, sizeof(buffer));

        io_uring_cqe* cqe;
        io_uring_wait_cqe(&ring_, &cqe);

        int bytes_read = cqe->res;
        if (bytes_read > 0) {
            process_message(buffer, static_cast<size_t>(bytes_read));
        }

        io_uring_cqe_seen(&ring_, cqe);
    }
}

bool FeedHandler::check_sequence(uint64_t sequence) {
    if (sequence < expected_sequence_) {
        ++duplicates_dropped_;
        return false;
    }
    if (sequence > expected_sequence_) {
        ++gaps_detected_;
    }
    expected_sequence_ = sequence + 1;
    return true;
}


}  // namespace engine