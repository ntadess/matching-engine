#include "messages.hpp"

#include <cstring>

namespace engine {

void encode_add_message(const AddMessage& msg, uint8_t* buffer) {
    buffer[0] = static_cast<uint8_t>(msg.type);
    std::memcpy(buffer + 1, &msg.id, sizeof(msg.id));
    buffer[9] = static_cast<uint8_t>(msg.side);
    std::memcpy(buffer + 10, &msg.price_ticks, sizeof(msg.price_ticks));
    std::memcpy(buffer + 14, &msg.quantity, sizeof(msg.quantity));
}

AddMessage decode_add_message(const uint8_t* buffer) {
    AddMessage msg;
    msg.type = static_cast<MessageType>(buffer[0]);
    std::memcpy(&msg.id, buffer + 1, sizeof(msg.id));
    msg.side = static_cast<Side>(buffer[9]);
    std::memcpy(&msg.price_ticks, buffer + 10, sizeof(msg.price_ticks));
    std::memcpy(&msg.quantity, buffer + 14, sizeof(msg.quantity));
    return msg;
}

void encode_cancel_message(const CancelMessage& msg, uint8_t* buffer) {
    buffer[0] = static_cast<uint8_t>(msg.type);
    std::memcpy(buffer + 1, &msg.id, sizeof(msg.id));
}

CancelMessage decode_cancel_message(const uint8_t* buffer) {
    CancelMessage msg;
    msg.type = static_cast<MessageType>(buffer[0]);
    std::memcpy(&msg.id, buffer + 1, sizeof(msg.id));
    return msg;
}

}  // namespace engine