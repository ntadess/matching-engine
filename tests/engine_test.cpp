// tests/engine_test.cpp
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>

#include "engine.hpp"
#include "messages.hpp"

using engine::AddMessage;
using engine::CancelMessage;
using engine::Engine;
using engine::Side;

namespace {

void send_udp(uint16_t port, const uint8_t* data, size_t len) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(fd, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    ssize_t sent = sendto(fd, data, len, 0, reinterpret_cast<sockaddr*>(&addr),
                           sizeof(addr));
    ASSERT_EQ(sent, static_cast<ssize_t>(len));
    close(fd);
}

}  // namespace

TEST(EngineEndToEnd, AddMessageFlowsThroughFullPipeline) {
    constexpr uint16_t kPort = 46001;
    Engine engine(kPort, 10000, 500);

    engine.start();

    AddMessage msg;
    msg.sequence = 0;
    msg.id = 1;
    msg.side = Side::Bid;
    msg.price_ticks = 9990;
    msg.quantity = 25;

    uint8_t buffer[engine::ADD_MESSAGE_SIZE];
    engine::encode_add_message(msg, buffer);
    send_udp(kPort, buffer, sizeof(buffer));


    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto best_bid = engine.book().best_bid_price();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_EQ(*best_bid, 9990);

    engine.stop();
}

TEST(EngineEndToEnd, AddThenCancelRemovesOrder) {
    constexpr uint16_t kPort = 46002;
    Engine engine(kPort, 10000, 500);

    engine.start();

    AddMessage add;
    add.sequence = 0;
    add.id = 2;
    add.side = Side::Ask;
    add.price_ticks = 10010;
    add.quantity = 15;

    uint8_t add_buf[engine::ADD_MESSAGE_SIZE];
    engine::encode_add_message(add, add_buf);
    send_udp(kPort, add_buf, sizeof(add_buf));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_TRUE(engine.book().best_ask_price().has_value());
    EXPECT_EQ(*engine.book().best_ask_price(), 10010);

    CancelMessage cancel;
    cancel.sequence = 1;
    cancel.id = 2;

    uint8_t cancel_buf[engine::CANCEL_MESSAGE_SIZE];
    engine::encode_cancel_message(cancel, cancel_buf);
    send_udp(kPort, cancel_buf, sizeof(cancel_buf));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_FALSE(engine.book().best_ask_price().has_value());

    engine.stop();
}

TEST(EngineEndToEnd, StopIsCleanAndJoinsBothThreads) {
    constexpr uint16_t kPort = 46003;
    Engine engine(kPort, 10000, 500);

    engine.start();
    
    engine.stop();
}

TEST(EngineEndToEnd, RecorderCapturesRealLatency) {
    constexpr uint16_t kPort = 46004;
    Engine engine(kPort, 10000, 500, 100);

    engine.start();

    AddMessage msg;
    msg.sequence = 0;
    msg.id = 1;
    msg.side = Side::Bid;
    msg.price_ticks = 9990;
    msg.quantity = 25;

    uint8_t buffer[engine::ADD_MESSAGE_SIZE];
    engine::encode_add_message(msg, buffer);
    send_udp(kPort, buffer, sizeof(buffer));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    double wire_to_book = engine.recorder().wire_to_book_percentile(0.5);
    EXPECT_GT(wire_to_book, 0.0);
   
    EXPECT_LT(wire_to_book, 10'000'000.0);

    engine.stop();
}