#include <catch2/catch_test_macros.hpp>

#include "core/cue_time.hpp"

using rm::MusicalContext;
using rm::parse_cue_time;
using rm::parse_duration;

namespace {
const MusicalContext kNoGrid{};
const MusicalContext k120{120.0, 4, 0};
}  // namespace

TEST_CASE("absolute cue times", "[cue_time]") {
  REQUIRE(parse_cue_time("0:00", kNoGrid) == 0);
  REQUIRE(parse_cue_time("0:45.500", kNoGrid) == 2'184'000);
  REQUIRE(parse_cue_time("1:02:03", kNoGrid) == 178'704'000);
  REQUIRE(parse_cue_time("12.5s", kNoGrid) == 600'000);
  REQUIRE(parse_cue_time("  0:01  ", kNoGrid) == 48'000);
}

TEST_CASE("invalid absolute cue times are rejected", "[cue_time]") {
  for (const char* bad : {"", "abc", "1:75", "1:60", "-1s", "12.5", "1:2:3:4", "0:-5", "1:61:00"}) {
    INFO(bad);
    REQUIRE_FALSE(parse_cue_time(bad, kNoGrid).has_value());
  }
}

TEST_CASE("musical cue times at 120 bpm 4/4", "[cue_time]") {
  REQUIRE(parse_cue_time("bar 1", k120) == 0);
  REQUIRE(parse_cue_time("bar 2", k120) == 96'000);
  REQUIRE(parse_cue_time("bar 17 beat 3.5", k120) == 1'596'000);
  REQUIRE(parse_cue_time("BAR 2 BEAT 1", k120) == 96'000);
}

TEST_CASE("musical times honour downbeat offset and non-round bpm", "[cue_time]") {
  const MusicalContext offset{120.0, 4, 48'000};
  REQUIRE(parse_cue_time("bar 1", offset) == 48'000);
  const MusicalContext odd{122.0, 4, 0};
  REQUIRE(parse_cue_time("bar 2", odd) == 94'426);
  const MusicalContext waltz{120.0, 3, 0};
  REQUIRE(parse_cue_time("bar 2", waltz) == 72'000);
}

TEST_CASE("invalid musical cue times are rejected", "[cue_time]") {
  REQUIRE_FALSE(parse_cue_time("bar 3", kNoGrid).has_value());
  REQUIRE_FALSE(parse_cue_time("bar 0", k120).has_value());
  REQUIRE_FALSE(parse_cue_time("bar x", k120).has_value());
  REQUIRE_FALSE(parse_cue_time("bar 1 beat 0.5", k120).has_value());
  REQUIRE_FALSE(parse_cue_time("bar 1 beat 5", k120).has_value());
  REQUIRE_FALSE(parse_cue_time("bar 1 beat", k120).has_value());
  REQUIRE_FALSE(parse_cue_time("bar 1 foo 2", k120).has_value());
}

TEST_CASE("durations", "[cue_time]") {
  REQUIRE(parse_duration("8 bars", k120) == 768'000);
  REQUIRE(parse_duration("1 bar", k120) == 96'000);
  REQUIRE(parse_duration("16 beats", k120) == 384'000);
  REQUIRE(parse_duration("30s", kNoGrid) == 1'440'000);
  REQUIRE(parse_duration("0.5 bars", k120) == 48'000);
}

TEST_CASE("invalid durations are rejected", "[cue_time]") {
  REQUIRE_FALSE(parse_duration("", k120).has_value());
  REQUIRE_FALSE(parse_duration("0 bars", k120).has_value());
  REQUIRE_FALSE(parse_duration("-2 bars", k120).has_value());
  REQUIRE_FALSE(parse_duration("8 bars", kNoGrid).has_value());
  REQUIRE_FALSE(parse_duration("8 parsecs", k120).has_value());
  REQUIRE_FALSE(parse_duration("8", k120).has_value());
}
