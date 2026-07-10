#pragma once

#include <cstdint>
#include <cstddef>
#include <variant>
#include "order_types.hpp"

namespace engine {



enum class MessageType: uint8_t { Add, Cancel };

// separate struct for add messages since they have more fields
struct AddMessage {
    MessageType type = MessageType::Add;
    uint64_t sequence;
    uint64_t id;
    Side side;
    int32_t price_ticks;
    uint32_t quantity;
};
struct CancelMessage {
    MessageType type = MessageType::Cancel;
    uint64_t sequence;
    uint64_t id;
};

using FeedMessage = std::variant<AddMessage, CancelMessage>;

constexpr size_t ADD_MESSAGE_SIZE = 26;
constexpr size_t CANCEL_MESSAGE_SIZE = 17;

void encode_add_message(const AddMessage& msg, uint8_t* buffer);
AddMessage decode_add_message(const uint8_t* buffer);

void encode_cancel_message(const CancelMessage& msg, uint8_t* buffer);
CancelMessage decode_cancel_message(const uint8_t* buffer);

}