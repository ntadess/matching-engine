#pragma once


#include <cstdint>

namespace engine {

enum class Side: uint8_t{ Bid, Ask };

struct Order {
    uint64_t id;
    int32_t price_ticks;
    uint32_t quantity;
    Side side;
    Order* prev = nullptr;
    Order* next = nullptr;
};


struct PriceLevel{
    Order* head = nullptr;
    Order* tail = nullptr;

    uint32_t total_quantity = 0;

    bool empty() const { return head == nullptr; }
};


} // namespace engine