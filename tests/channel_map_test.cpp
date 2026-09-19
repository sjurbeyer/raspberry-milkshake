#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "core/channel_map.hpp"

using namespace rm;

TEST_CASE("only stereo and 5.1 are supported", "[channel_map]") {
  REQUIRE(layout_from_channels(2) == Layout::Stereo);
  REQUIRE(layout_from_channels(6) == Layout::Surround51);
  for (const int n : {0, 1, 3, 4, 5, 7, 8}) REQUIRE_FALSE(layout_from_channels(n).has_value());
  REQUIRE(channel_count(Layout::Stereo) == 2);
  REQUIRE(channel_count(Layout::Surround51) == 6);
}

TEST_CASE("5.1 file order maps to ALSA HDMI order", "[channel_map]") {
  // File: FL FR FC LFE BL BR  ->  ALSA: FL FR RL RR FC LFE
  REQUIRE(file_to_alsa_map(Layout::Surround51) == std::vector<int>{0, 1, 4, 5, 2, 3});

  // Label each channel with its file index so the result shows where it went.
  const std::vector<float> in = {0, 1, 2, 3, 4, 5,  10, 11, 12, 13, 14, 15};
  std::vector<float> out(in.size());
  remap_to_alsa(in.data(), out.data(), 2, Layout::Surround51);
  REQUIRE(out == std::vector<float>{0, 1, 4, 5, 2, 3,  10, 11, 14, 15, 12, 13});
}

TEST_CASE("stereo is left untouched", "[channel_map]") {
  const std::vector<float> in = {0.1f, 0.2f, 0.3f, 0.4f};
  std::vector<float> out(in.size());
  remap_to_alsa(in.data(), out.data(), 2, Layout::Stereo);
  REQUIRE(out == in);
}
