#include "engine.hpp"


namespace engine{

Engine::Engine(uint16_t port, int32_t reference_price_ticks, int32_t window_ticks,
               size_t expected_message_count)
    : book_(reference_price_ticks, window_ticks),
      recorder_(expected_message_count),
      feed_handler_(queue_, port, &recorder_),
      matcher_(queue_, book_, &recorder_) {}

// engine.cpp
void Engine::start(int cpu_core) {
    matcher_.start(cpu_core);
    feed_handler_.start(cpu_core);
}

void Engine::stop() {
    feed_handler_.stop();   // stop producer first
    matcher_.stop();        
}

}