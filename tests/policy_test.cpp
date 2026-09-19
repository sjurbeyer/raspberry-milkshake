#include <catch2/catch_test_macros.hpp>

#include "core/policy.hpp"

using namespace rm;

TEST_CASE("browse actions when nothing is playing", "[policy]") {
  for (const auto type : {CollectionType::Album, CollectionType::Playlist, CollectionType::Interactive}) {
    const BrowseActions a = browse_actions(TransportState::Idle, type);
    REQUIRE(a.preview_allowed);
    REQUIRE(a.play_now);
    REQUIRE_FALSE(a.add_to_queue);
  }
}

TEST_CASE("browse actions while something is playing", "[policy]") {
  for (const auto type : {CollectionType::Album, CollectionType::Playlist}) {
    const BrowseActions a = browse_actions(TransportState::Playing, type);
    REQUIRE_FALSE(a.preview_allowed);
    REQUIRE(a.play_now);
    REQUIRE(a.add_to_queue);
  }
  const BrowseActions interactive = browse_actions(TransportState::Playing, CollectionType::Interactive);
  REQUIRE_FALSE(interactive.preview_allowed);
  REQUIRE(interactive.play_now);
  REQUIRE_FALSE(interactive.add_to_queue);
}

TEST_CASE("queue starts idle", "[policy]") {
  Queue q;
  REQUIRE(q.state() == TransportState::Idle);
  REQUIRE_FALSE(q.current().has_value());
  REQUIRE_FALSE(q.next().has_value());
  REQUIRE(q.state() == TransportState::Idle);
}

TEST_CASE("adding to an idle queue starts playback", "[policy]") {
  Queue q;
  q.add("night-drive");
  REQUIRE(q.state() == TransportState::Playing);
  REQUIRE(q.current() == "night-drive");
  REQUIRE(q.upcoming().empty());
}

TEST_CASE("queued items play in order and the queue ends idle", "[policy]") {
  Queue q;
  q.play_now("a");
  q.add("b");
  q.add("c");
  REQUIRE(q.current() == "a");
  REQUIRE(q.upcoming().size() == 2);
  REQUIRE(q.next() == "b");
  REQUIRE(q.next() == "c");
  REQUIRE_FALSE(q.next().has_value());
  REQUIRE(q.state() == TransportState::Idle);
}

TEST_CASE("play now interrupts the current item and keeps the queue", "[policy]") {
  Queue q;
  q.play_now("a");
  q.add("b");
  q.play_now("x");
  REQUIRE(q.current() == "x");
  REQUIRE(q.upcoming().size() == 1);
  REQUIRE(q.next() == "b");
}
