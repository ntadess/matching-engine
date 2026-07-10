#pragma once

#include <cstdint>
#include <liburing.h>
#include <atomic>
#include <thread>
#include <pthread.h>
#include "spsc_queue.hpp"   
#include "messages.hpp"
#include "latency_recorder.hpp"

namespace engine {

class FeedHandler {
public:
    FeedHandler(SpscQueue<FeedMessage, 1024>& queue, uint16_t port, LatencyRecorder* recorder = nullptr);  
    ~FeedHandler();

    FeedHandler(const FeedHandler&) = delete;
    FeedHandler& operator=(const FeedHandler&) = delete;

    void run_for(int max_messages);

    uint64_t gaps_detected() const { return gaps_detected_; }
    uint64_t duplicates_dropped() const { return duplicates_dropped_; }

    void start(int cpu_core = -1);
    void stop();

private:
    
    SpscQueue<FeedMessage, 1024>& queue_;  
    LatencyRecorder* recorder_ = nullptr;
    int socket_fd_ = -1;
    io_uring ring_{};

    uint64_t expected_sequence_ = 0;
    uint64_t gaps_detected_ = 0;
    uint64_t duplicates_dropped_ = 0;
    static constexpr int kNumBuffers = 8;

    std::array<std::array<uint8_t, ADD_MESSAGE_SIZE>, kNumBuffers> buffers_{};
    void submit_read(uint8_t* buffer, size_t buffer_len);
    void submit_read_for_buffer(int buffer_index);

    bool check_sequence(uint64_t sequence);

    void setup_socket(uint16_t port);
    
    void process_message(const uint8_t* buffer, size_t len);

    void run_forever();
    std::thread producer_thread_;
    std::atomic<bool> running_ {false};
};
}  // namespace engine