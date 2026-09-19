#include <catch2/catch_test_macros.hpp>

#include "core/clock.hpp"

TEST_CASE("VirtualClock starts at zero and only moves when told", "[clock]") {
  rm::VirtualClock clock;
  REQUIRE(clock.now() == 0);
  clock.advance(1024);
  clock.advance(1024);
  REQUIRE(clock.now() == 2048);
  clock.set(48000);
  REQUIRE(clock.now() == rm::kSampleRate);
}
