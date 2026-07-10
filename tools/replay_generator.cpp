// tools/replay_generator.cpp
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

#include "messages.hpp"
#include "order_types.hpp"

using namespace engine;

namespace {

int open_udp_socket() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::runtime_error("failed to create socket");
    }
    return fd;
}

void send_bytes(int fd, const sockaddr_in& addr, const uint8_t* data, size_t len) {
    ssize_t sent = sendto(fd, data, len, 0,
                           reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (sent != static_cast<ssize_t>(len)) {
        throw std::runtime_error("sendto failed or sent partial message");
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: replay_generator <port> <message_count> [rate_per_sec]\n";
        return 1;
    }

    uint16_t port = static_cast<uint16_t>(std::stoi(argv[1]));
    uint64_t message_count = std::stoull(argv[2]);
    double rate_per_sec = argc >= 4 ? std::stod(argv[3]) : 50000.0;

    constexpr int32_t kReferencePrice = 10000;
    constexpr double kPriceStdDevTicks = 5.0;
    constexpr uint32_t kMinQuantity = 1;
    constexpr uint32_t kMaxQuantity = 500;
    constexpr double kAddProbability = 0.70;  // 70% add, 30% cancel

    std::mt19937 rng(42);  // fixed seed 
    std::normal_distribution<double> price_offset_dist(0.0, kPriceStdDevTicks);
    std::uniform_int_distribution<uint32_t> quantity_dist(kMinQuantity, kMaxQuantity);
    std::bernoulli_distribution add_or_cancel_dist(kAddProbability);
    std::bernoulli_distribution side_dist(0.5);

    int fd = open_udp_socket();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    
    std::vector<uint64_t> live_order_ids;

    auto interval = std::chrono::duration<double>(1.0 / rate_per_sec);
    auto next_send_time = std::chrono::steady_clock::now();

    uint8_t add_buf[ADD_MESSAGE_SIZE];
    uint8_t cancel_buf[CANCEL_MESSAGE_SIZE];

    for (uint64_t sequence = 0; sequence < message_count; ++sequence) {
        
        bool do_cancel = !live_order_ids.empty() && !add_or_cancel_dist(rng);

        if (do_cancel) {
            std::uniform_int_distribution<size_t> pick(0, live_order_ids.size() - 1);
            size_t idx = pick(rng);
            uint64_t id = live_order_ids[idx];

            
            live_order_ids[idx] = live_order_ids.back();
            live_order_ids.pop_back();

            CancelMessage msg;
            msg.sequence = sequence;
            msg.id = id;

            encode_cancel_message(msg, cancel_buf);
            send_bytes(fd, addr, cancel_buf, sizeof(cancel_buf));
        } else {
            AddMessage msg;
            msg.sequence = sequence;
            msg.id = sequence;  
            msg.side = side_dist(rng) ? Side::Bid : Side::Ask;

            int32_t offset = static_cast<int32_t>(std::round(price_offset_dist(rng)));
            msg.price_ticks = kReferencePrice + offset;
            msg.quantity = quantity_dist(rng);

            live_order_ids.push_back(msg.id);

            encode_add_message(msg, add_buf);
            send_bytes(fd, addr, add_buf, sizeof(add_buf));
        }

        // pace to the target rate
        next_send_time += std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);
        std::this_thread::sleep_until(next_send_time);

        if (sequence % 10000 == 0) {
            std::cerr << "sent " << sequence << " / " << message_count << "\n";
        }
    }

    close(fd);
    std::cerr << "done: sent " << message_count << " messages\n";
    return 0;
}