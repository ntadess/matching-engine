#include <gtest/gtest.h>
#include "messages.hpp"

using engine::AddMessage;
using engine::CancelMessage;
using engine::MessageType;
using engine::Side;


TEST(MessagesRoundTrip, AddMessageSurvivesEncodeDecode) {
    AddMessage original;
    original.id = 123456789;
    original.side = Side::Bid;
    original.price_ticks = 10005;
    original.quantity = 100;

    uint8_t buffer[engine::ADD_MESSAGE_SIZE];
    engine::encode_add_message(original, buffer);
    AddMessage decoded = engine::decode_add_message(buffer);

    EXPECT_EQ(decoded.type, MessageType::Add);
    EXPECT_EQ(decoded.id, original.id);
    EXPECT_EQ(decoded.side, original.side);
    EXPECT_EQ(decoded.price_ticks, original.price_ticks);
    EXPECT_EQ(decoded.quantity, original.quantity);
}


TEST(MessagesRoundTrip, CancelMessageSurvivesEncodeDecode) {
    CancelMessage original;
    original.id = 987654321;

    uint8_t buffer[engine::CANCEL_MESSAGE_SIZE];
    engine::encode_cancel_message(original, buffer);
    CancelMessage decoded = engine::decode_cancel_message(buffer);

    EXPECT_EQ(decoded.type, MessageType::Cancel);
    EXPECT_EQ(decoded.id, original.id);
}

TEST(MessagesRoundTrip, AddMessageSurvivesBoundaryValues) {
    AddMessage original;
    original.id = UINT64_MAX;
    original.side = Side::Ask;
    original.price_ticks = INT32_MIN;
    original.quantity = UINT32_MAX;

    uint8_t buffer[engine::ADD_MESSAGE_SIZE];
    engine::encode_add_message(original, buffer);
    AddMessage decoded = engine::decode_add_message(buffer);

    EXPECT_EQ(decoded.id, original.id);
    EXPECT_EQ(decoded.side, original.side);
    EXPECT_EQ(decoded.price_ticks, original.price_ticks);
    EXPECT_EQ(decoded.quantity, original.quantity);
}

TEST(MessagesRoundTrip, CancelMessageSurvivesBoundaryId) {
    CancelMessage original;
    original.id = 0;

    uint8_t buffer[engine::CANCEL_MESSAGE_SIZE];
    engine::encode_cancel_message(original, buffer);
    CancelMessage decoded = engine::decode_cancel_message(buffer);

    EXPECT_EQ(decoded.id, 0u);
}

TEST(MessagesRoundTrip, AddAndCancelBuffersDoNotOverlapWhenPackedSequentially) {
    uint8_t stream[engine::ADD_MESSAGE_SIZE + engine::CANCEL_MESSAGE_SIZE];

    AddMessage add_msg;
    add_msg.id = 111;
    add_msg.side = Side::Bid;
    add_msg.price_ticks = 10000;
    add_msg.quantity = 50;
    engine::encode_add_message(add_msg, stream);

    CancelMessage cancel_msg;
    cancel_msg.id = 111;
    engine::encode_cancel_message(cancel_msg, stream + engine::ADD_MESSAGE_SIZE);

    AddMessage decoded_add = engine::decode_add_message(stream);
    CancelMessage decoded_cancel =
        engine::decode_cancel_message(stream + engine::ADD_MESSAGE_SIZE);

    EXPECT_EQ(decoded_add.id, 111u);
    EXPECT_EQ(decoded_add.quantity, 50u);
    EXPECT_EQ(decoded_cancel.type, MessageType::Cancel);
    EXPECT_EQ(decoded_cancel.id, 111u);
}