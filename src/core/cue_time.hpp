#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "core/clock.hpp"

namespace rm {

// Fixed-tempo musical grid of one track. bpm == 0 means "no grid": musical times are rejected.
struct MusicalContext {
  double bpm = 0.0;
  int beats_per_bar = 4;
  SamplePos downbeat_offset = 0;  // sample position of bar 1, beat 1

  bool operator==(const MusicalContext&) const = default;
};

using TimeResult = std::expected<SamplePos, std::string>;

// Absolute: "0:45.500", "1:02:03", "12.5s".
// Musical:  "bar 17", "bar 17 beat 3.5" (1-based, fractional beats allowed).
TimeResult parse_cue_time(std::string_view text, const MusicalContext& ctx);

// "8 bars", "16 beats", "30s". Result is at least one sample.
TimeResult parse_duration(std::string_view text, const MusicalContext& ctx);

}  // namespace rm
