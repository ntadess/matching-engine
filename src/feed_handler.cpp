#include "feed_handler.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <array>

namespace engine {

FeedHandler::FeedHandler(SpscQueue<FeedMessage, 1024>& queue, uint16_t port,
                          LatencyRecorder* recorder)
    : queue_(queue), recorder_(recorder) {
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


void FeedHandler::process_message(const uint8_t* data, size_t len) {
    if (len == 0) {
        return;
    }

    MessageType type = static_cast<MessageType>(data[0]);

    uint64_t sequence;
    std::memcpy(&sequence, data + 1, sizeof(sequence));

    if (recorder_) recorder_->record_received(sequence);   // add — as early as possible

    if (!check_sequence(sequence)) {
        return;
    }

    if (type == MessageType::Add && len >= ADD_MESSAGE_SIZE) {
        AddMessage msg = decode_add_message(data);
        if (recorder_) recorder_->record_decoded(sequence);   // add
        queue_.push(FeedMessage{msg});
    } else if (type == MessageType::Cancel && len >= CANCEL_MESSAGE_SIZE) {
        CancelMessage msg = decode_cancel_message(data);
        if (recorder_) recorder_->record_decoded(sequence);   // add
        queue_.push(FeedMessage{msg});
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


// feed_handler.cpp
void FeedHandler::start(int cpu_core) {
    running_.store(true, std::memory_order_relaxed);
    producer_thread_ = std::thread(&FeedHandler::run_forever, this);

    if (cpu_core >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_core, &cpuset);
        pthread_setaffinity_np(producer_thread_.native_handle(), sizeof(cpu_set_t), &cpuset);
    }
}

void FeedHandler::stop() {
    running_.store(false, std::memory_order_relaxed);
    if (producer_thread_.joinable()) {
        producer_thread_.join();
    }
}


void FeedHandler::submit_read(uint8_t* buffer, size_t buffer_len) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    io_uring_prep_recv(sqe, socket_fd_, buffer, buffer_len, 0);
    io_uring_submit(&ring_);
}

void FeedHandler::submit_read_for_buffer(int buffer_index) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        // submission queue full -- shouldn't normally happen at steady
        // 1-in/1-out balance, but guard rather than crash
        return;
    }
    io_uring_prep_recv(sqe, socket_fd_, buffers_[buffer_index].data(),
                        buffers_[buffer_index].size(), 0);
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(static_cast<intptr_t>(buffer_index)));
    io_uring_submit(&ring_);
}

void FeedHandler::run_forever() {
    // prime the pipe: submit ALL buffers' reads up front, so kNumBuffers
    // reads are in flight simultaneously before we wait on anything
    for (int i = 0; i < kNumBuffers; ++i) {
        submit_read_for_buffer(i);
    }

    while (running_.load(std::memory_order_relaxed)) {
        io_uring_cqe* cqe;
        __kernel_timespec ts{.tv_sec = 0, .tv_nsec = 100'000'000};  // 100ms
        int ret = io_uring_wait_cqe_timeout(&ring_, &cqe, &ts);

        if (ret == -ETIME) {
            continue;  // no completion yet, recheck running_
        }

        int buffer_index = static_cast<int>(reinterpret_cast<intptr_t>(io_uring_cqe_get_data(cqe)));
        int bytes_read = cqe->res;

        if (bytes_read > 0) {
            process_message(buffers_[buffer_index].data(), static_cast<size_t>(bytes_read));
        }

        io_uring_cqe_seen(&ring_, cqe);

        // immediately resubmit a read on this same buffer -- keeps
        // kNumBuffers reads continuously in flight rather than draining
        // to zero and refilling in lockstep
        submit_read_for_buffer(buffer_index);
    }
}

}  // namespace engine