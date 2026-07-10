#include "engine.hpp"


namespace engine{

Engine::Engine(uint16_t port, int32_t reference_price_ticks, int32_t window_ticks)
    : book_(reference_price_ticks, window_ticks),
      feed_handler_(queue_, port),
      matcher_(queue_, book_) {}

void Engine::start() {
    matcher_.start();       // consumer first
    feed_handler_.start();  // producer second
}

void Engine::stop() {
    feed_handler_.stop();   // stop producer first
    matcher_.stop();        
}

}