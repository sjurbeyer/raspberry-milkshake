#include <catch2/catch_test_macros.hpp>

#include "core/policy.hpp"

using namespace rm;

TEST_CASE("pausing an item moves the transport to paused and resuming plays again", "[policy][pause]") {
  Queue q;
  q.play_now("a");
  REQUIRE(q.state() == TransportState::Playing);
  q.pause();
  REQUIRE(q.state() == TransportState::Paused);
  REQUIRE(q.current() == "a");
  q.resume();
  REQUIRE(q.state() == TransportState::Playing);
}

TEST_CASE("pause does nothing while idle", "[policy][pause]") {
  Queue q;
  q.pause();
  REQUIRE(q.state() == TransportState::Idle);
  q.play_now("a");  // and does not linger to pause the next item
  REQUIRE(q.state() == TransportState::Playing);
}

TEST_CASE("the queue is untouched by pausing", "[policy][pause]") {
  Queue q;
  q.play_now("a");
  q.add("b");
  q.pause();
  REQUIRE(q.upcoming().size() == 1);
  q.resume();
  REQUIRE(q.upcoming().size() == 1);
}

TEST_CASE("changing the current item ends the pause", "[policy][pause]") {
  Queue q;
  q.play_now("a");
  q.add("b");
  q.pause();
  REQUIRE(q.next() == "b");  // next while paused starts the next item playing
  REQUIRE(q.state() == TransportState::Playing);

  q.pause();
  q.play_now("c");  // so does play now
  REQUIRE(q.state() == TransportState::Playing);
  REQUIRE(q.current() == "c");
}

TEST_CASE("running out of items while paused goes idle, not paused", "[policy][pause]") {
  Queue q;
  q.play_now("a");
  q.pause();
  REQUIRE_FALSE(q.next().has_value());
  REQUIRE(q.state() == TransportState::Idle);
  q.play_now("b");
  REQUIRE(q.state() == TransportState::Playing);
}

TEST_CASE("clear stops the current item and empties the queue", "[policy][pause]") {
  Queue q;
  q.play_now("a");
  q.add("b");
  q.add("c");
  q.pause();
  q.clear();
  REQUIRE(q.state() == TransportState::Idle);
  REQUIRE_FALSE(q.current().has_value());
  REQUIRE(q.upcoming().empty());
  q.play_now("d");
  REQUIRE(q.state() == TransportState::Playing);  // the old pause did not survive
}

TEST_CASE("a paused item counts as something playing for the browse rules", "[policy][pause]") {
  for (const auto type : {CollectionType::Album, CollectionType::Playlist}) {
    const BrowseActions a = browse_actions(TransportState::Paused, type);
    REQUIRE_FALSE(a.preview_allowed);
    REQUIRE(a.add_to_queue);
    REQUIRE(a.play_now);
  }
  const BrowseActions interactive = browse_actions(TransportState::Paused, CollectionType::Interactive);
  REQUIRE_FALSE(interactive.preview_allowed);
  REQUIRE_FALSE(interactive.add_to_queue);
  REQUIRE(interactive.play_now);
}
