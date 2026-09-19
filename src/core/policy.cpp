#include "core/policy.hpp"

namespace rm {

BrowseActions browse_actions(TransportState state, CollectionType type) {
  if (state == TransportState::Idle) return {true, false, true};
  return {false, type != CollectionType::Interactive, true};
}

void Queue::add(std::string id) {
  if (!current_) {
    current_ = std::move(id);
    return;
  }
  upcoming_.push_back(std::move(id));
}

void Queue::play_now(std::string id) {
  current_ = std::move(id);
  paused_ = false;
}

void Queue::pause() {
  if (current_) paused_ = true;
}

void Queue::resume() { paused_ = false; }

void Queue::clear() {
  current_.reset();
  upcoming_.clear();
  paused_ = false;
}

std::optional<std::string> Queue::next() {
  paused_ = false;
  if (upcoming_.empty()) {
    current_.reset();
    return std::nullopt;
  }
  current_ = std::move(upcoming_.front());
  upcoming_.pop_front();
  return current_;
}

}  // namespace rm
