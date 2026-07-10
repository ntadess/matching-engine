// ring buffer class
#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>

namespace engine {
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    SpscQueue() = default;

    bool push(T item); // producer thread returns false if queue full
    std::optional<T> pop(); // consumer thread returns empty optional if queue empty

private:
    std::array<T, Capacity> buffer_; // ring buffer no alloc on hot path

    alignas(64) std::atomic<std::size_t> write_index_{0}; // align for false sharing prevent
    alignas(64) std::atomic<std::size_t> read_index_{0};
};

template <typename T, std::size_t Capacity>
bool SpscQueue<T, Capacity>::push(T item) { 
    //lock free acquire and release

    // read write_index relaxed
    auto write_index = write_index_.load(std::memory_order_relaxed);

    auto read_index = read_index_.load(std::memory_order_acquire);

    // write write_index to publish
    if ((write_index - read_index) != Capacity) {
        buffer_[write_index & (Capacity - 1)] = item;
        write_index_.store(write_index + 1, std::memory_order_release);
        return true;
    }

    return false;
}


template <typename T, std::size_t Capacity>
std::optional<T> SpscQueue<T, Capacity>::pop() {
    auto read_index = read_index_.load(std::memory_order_relaxed);
    auto write_index = write_index_.load(std::memory_order_acquire);

    if (read_index != write_index) {
        auto item = std::move(buffer_[read_index & (Capacity - 1)]);
        read_index_.store(read_index + 1, std::memory_order_release);
        return item;
    }

    return std::nullopt;

}
}
