#include "order_book.hpp"

namespace engine {

OrderBook::OrderBook(int32_t reference_price_ticks, int32_t window_ticks)
    : reference_price_ticks_(reference_price_ticks),
      window_ticks_(window_ticks),
      bid_levels_(static_cast<size_t>(2 * window_ticks + 1)),
      ask_levels_(static_cast<size_t>(2 * window_ticks + 1)) {}

}

int OrderBook::index_for_price(int32_t price_ticks) const { // replace with resizing logic later
    int index = price_ticks - reference_price_ticks_ + window_ticks_;

    if (index < 0 || index >= static_cast<int>(bid_levels_.size())) {
        throw std::out_of_range("price ticks outside current book window");
    }

    return index;
}


int32_t OrderBook::price_for_index(int index) const {
    return static_cast<int32_t>(index - window_ticks_) + reference_price_ticks_;
}

PriceLevel& OrderBook::level_for(Side side, int index) {
    return side == Side::Bid ? bid_levels_[index] : ask_levels_[index];
}

const PriceLevel& OrderBook::level_for(Side side, int index) const { // const version for const functinos
    return side == Side::Bid ? bid_levels_[index] : ask_levels_[index];
}

void OrderBook::insert_into_level(Order* order, int index) {
    PriceLevel& level = level_for(order->side, index);

    order->prev = level.tail;
    order->next = nullptr;

    if (level.tail) {
        level.tail->next = order;
    } else {
        level.head = order; 
    }

    level.tail = order;
    level.total_quantity += order->quantity;

    if (order->side == Side::Bid) {
        if (best_bid_index_ == -1 || index > best_bid_index_) {
            best_bid_index_ = index;
        }
    } else {
        if (best_ask_index_ == -1 || index < best_ask_index_) {
            best_ask_index_ = index;
        }
    }
}

void OrderBook::unlink_from_level(Order* order, int index) {
    PriceLevel& level = level_for(order->side, index);

    if (order->prev) {
        order->prev->next = order->next;
    } else {
        level.head = order->next;
    }

    if (order->next) {
        order->next->prev = order->prev;
    } else {
        level.tail = order->prev;
    }

    level.total_quantity -= order->quantity;

    order->prev = nullptr;
    order->next = nullptr;

    if (level.empty()) {
        if (order->side == Side::Bid && index == best_bid_index_) {
            int i = index - 1;
            while (i >= 0 && bid_levels_[static_cast<size_t>(i)].empty()){
                --i;
            }
            best_bid_index_ = (i >= 0) ? i : -1;
        } else if (order->side == Side::Ask && index == best_ask_index_) {
            int i = index + 1;
            while (i < static_cast<int>(ask_levels_.size()) && ask_levels_[static_cast<size_t>(i)].empty()){
                ++i;
            }
            best_ask_index_ = (i < static_cast<int>(ask_levels_.size())) ? i : -1;
        }
    }
}

void OrderBook::add_order(uint64_t id, Side side, int32_t price_ticks, uint32_t quantity) {
    // figure out the index for the price index_for price
    // create oder object
    // add to order_lookup_
    // insert into level

    int index = index_for_price(price_ticks);
    auto order = std::make_unique<Order>();
    order->id = id;
    order->price_ticks = price_ticks;
    order->quantity = quantity;
    order->side = side;

    Order* raw = order.get();
    order_lookup_[id] = std::move(order);
    insert_into_level(raw, index);
}

bool OrderBook::cancel_order(uint64_t id) {
    // false if id not found

    auto it = order_lookup_.find(id);

    if (it == order_lookup_.end()) {
        return false;
    }
    Order* order = it->second.get();
    int index = index_for_price(order->price_ticks);
    unlink_from_level(order, index);

    order_lookup_.erase(it);
    return true;
}a

// namespace engine