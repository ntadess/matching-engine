#include "messages.hpp"

#include <cstring>

namespace engine {

void encode_add_message(const AddMessage& msg, uint8_t* buffer) {
    buffer[0] = static_cast<uint8_t>(msg.type);
    std::memcpy(buffer + 1, &msg.sequence, sizeof(msg.sequence));
    std::memcpy(buffer + 9, &msg.id, sizeof(msg.id));
    buffer[17] = static_cast<uint8_t>(msg.side);
    std::memcpy(buffer + 18, &msg.price_ticks, sizeof(msg.price_ticks));
    std::memcpy(buffer + 22, &msg.quantity, sizeof(msg.quantity));
}

AddMessage decode_add_message(const uint8_t* buffer) {
    AddMessage msg;
    msg.type = static_cast<MessageType>(buffer[0]);
    std::memcpy(&msg.sequence, buffer + 1, sizeof(msg.sequence));
    std::memcpy(&msg.id, buffer + 9, sizeof(msg.id));
    msg.side = static_cast<Side>(buffer[17]);
    std::memcpy(&msg.price_ticks, buffer + 18, sizeof(msg.price_ticks));
    std::memcpy(&msg.quantity, buffer + 22, sizeof(msg.quantity));
    return msg;
}

void encode_cancel_message(const CancelMessage& msg, uint8_t* buffer) {
    buffer[0] = static_cast<uint8_t>(msg.type);
    std::memcpy(buffer + 1, &msg.sequence, sizeof(msg.sequence));
    std::memcpy(buffer + 9, &msg.id, sizeof(msg.id));
}

CancelMessage decode_cancel_message(const uint8_t* buffer) {
    CancelMessage msg;
    msg.type = static_cast<MessageType>(buffer[0]);
    std::memcpy(&msg.sequence, buffer + 1, sizeof(msg.sequence));
    std::memcpy(&msg.id, buffer + 9, sizeof(msg.id));
    return msg;
}
}  // namespace engine