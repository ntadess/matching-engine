#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>

#include "order_types.hpp"


namespace engine {

class OrderBook {
public:
    OrderBook(int32_t reference_price_ticks, int32_t window_ticks);

    void add_order(uint64_t id, Side side, int32_t price_ticks, uint32_t quantity);
    bool cancel_order(uint64_t id);

    std::optional<int32_t> best_bid_price() const;
    std::optional<int32_t> best_ask_price() const;

    const PriceLevel& bid_level_at_price(int32_t price_ticks) const;
    const PriceLevel& ask_level_at_price(int32_t price_ticks) const;


private:
    int32_t reference_price_ticks_;
    int32_t window_ticks_;

    std::vector<PriceLevel> bid_levels_;
    std::vector<PriceLevel> ask_levels_;

    std::unordered_map<uint64_t, std::unique_ptr<Order>> order_lookup_;

    int best_bid_index_ = -1;
    int best_ask_index_ = -1;

    int index_for_price(int32_t price_ticks) const;
    int32_t price_for_index(int index) const;

    PriceLevel& level_for(Side side, int index);
    const PriceLevel& level_for(Side side, int index) const;

    void insert_into_level(Order *order, int index);
    void unlink_from_level(Order *order, int index);


};

}// namespace engine