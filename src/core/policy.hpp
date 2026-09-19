#pragma once

#include <deque>
#include <optional>
#include <string>

#include "core/manifest.hpp"

namespace rm {

enum class TransportState { Idle, Playing };

// What the browse (swipe) view offers for the collection on screen.
struct BrowseActions {
  bool preview_allowed = false;  // preview audio + visuals on the main output
  bool add_to_queue = false;
  bool play_now = false;         // starts it; interrupts whatever is playing
};

// Idle: preview is allowed and the collection can be started.
// Playing: previews are suppressed so the room is never interrupted, and the collection can be
// queued or started (interrupting). Interactive collections are open-ended, so they cannot be queued.
BrowseActions browse_actions(TransportState state, CollectionType type);

// Play queue of collection ids. `play_now` interrupts the current item and keeps the upcoming ones.
class Queue {
 public:
  void add(std::string id);
  void play_now(std::string id);
  // Moves to the next queued item, or to idle when the queue is empty. Returns the new current item.
  std::optional<std::string> next();

  TransportState state() const { return current_ ? TransportState::Playing : TransportState::Idle; }
  const std::optional<std::string>& current() const { return current_; }
  const std::deque<std::string>& upcoming() const { return upcoming_; }

 private:
  std::optional<std::string> current_;
  std::deque<std::string> upcoming_;
};

}  // namespace rm
