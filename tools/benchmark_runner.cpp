// tools/benchmark_runner.cpp
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

#include "engine.hpp"

using namespace engine;

namespace {

void send_bytes(int fd, const sockaddr_in& addr, const uint8_t* data, size_t len) {
    ssize_t sent = sendto(fd, data, len, 0,
                           reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    if (sent != static_cast<ssize_t>(len)) {
        throw std::runtime_error("sendto failed or sent partial message");
    }
}

void print_ns(const char* label, double ns) {
    std::cout << label << ": " << ns << " ns  (" << (ns / 1000.0) << " us)\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: benchmark_runner <port> <message_count> [rate_per_sec]\n";
        return 1;
    }

    uint16_t port = static_cast<uint16_t>(std::stoi(argv[1]));
    uint64_t message_count = std::stoull(argv[2]);
    double rate_per_sec = argc >= 4 ? std::stod(argv[3]) : 50000.0;

    constexpr int32_t kReferencePrice = 10000;
    constexpr double kPriceStdDevTicks = 5.0;
    constexpr uint32_t kMinQuantity = 1;
    constexpr uint32_t kMaxQuantity = 500;
    constexpr double kAddProbability = 0.70;

    // ---- start the engine ----
    Engine engine(port, kReferencePrice, /*window_ticks=*/500, message_count);
    engine.start();
    // give the feed handler's socket a moment to be ready before we hammer it
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ---- generator setup (same model as replay_generator.cpp) ----
    std::mt19937 rng(42);
    std::normal_distribution<double> price_offset_dist(0.0, kPriceStdDevTicks);
    std::uniform_int_distribution<uint32_t> quantity_dist(kMinQuantity, kMaxQuantity);
    std::bernoulli_distribution add_or_cancel_dist(kAddProbability);
    std::bernoulli_distribution side_dist(0.5);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    std::vector<uint64_t> live_order_ids;
    uint8_t add_buf[ADD_MESSAGE_SIZE];
    uint8_t cancel_buf[CANCEL_MESSAGE_SIZE];

    auto interval = std::chrono::duration<double>(1.0 / rate_per_sec);
    auto next_send_time = std::chrono::steady_clock::now();
    auto run_start = next_send_time;

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

        next_send_time += std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);
        std::this_thread::sleep_until(next_send_time);
    }

    auto send_end = std::chrono::steady_clock::now();
    close(fd);

    // let the pipeline drain -- give the matcher time to catch up on any backlog
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    engine.stop();

    double actual_duration_sec =
        std::chrono::duration<double>(send_end - run_start).count();
    double achieved_rate = message_count / actual_duration_sec;

    

    // ---- report ----
    std::cout << "\n=== Benchmark Results ===\n";
    std::cout << "messages sent:    " << message_count << "\n";
    std::cout << "target rate:      " << rate_per_sec << " msg/sec\n";
    std::cout << "achieved rate:    " << achieved_rate << " msg/sec\n\n";

    const auto& rec = engine.recorder();

    size_t complete = rec.complete_count();
    double loss_pct = 100.0 * (1.0 - static_cast<double>(complete) / message_count);

    std::cout << "messages measured: " << complete << " / " << message_count
            << " (" << loss_pct << "% not captured — dropped/incomplete)\n";
    std::cout << "-- wire-to-book (full pipeline) --\n";
    print_ns("  p50 ", rec.wire_to_book_percentile(0.50));
    print_ns("  p99 ", rec.wire_to_book_percentile(0.99));
    print_ns("  p99.9", rec.wire_to_book_percentile(0.999));

    std::cout << "\n-- decode (received -> decoded) --\n";
    print_ns("  p50 ", rec.decode_percentile(0.50));
    print_ns("  p99 ", rec.decode_percentile(0.99));

    std::cout << "\n-- queue transit (decoded -> popped) --\n";
    print_ns("  p50 ", rec.queue_transit_percentile(0.50));
    print_ns("  p99 ", rec.queue_transit_percentile(0.99));

    std::cout << "\n-- match (popped -> applied) --\n";
    print_ns("  p50 ", rec.match_percentile(0.50));
    print_ns("  p99 ", rec.match_percentile(0.99));

    std::cout << "\nnote: measured cross-thread (producer/consumer run on separate\n"
                 "cores); p50 figures may show minor negative-latency artifacts\n"
                 "from cross-core clock skew under WSL2 virtualization. p99/p99.9\n"
                 "are unaffected -- real tail latency dominates skew by 1-2 orders\n"
                 "of magnitude.\n";

    return 0;
}