#pragma once

#include <cstdint>
#include <vector>

#include "core/frame.hpp"

namespace rm {

// A point on the picture: u across (0 = left, 1 = right), v down (0 = top, 1 = bottom).
struct LedPoint {
  float u = 0;
  float v = 0;
};

struct LedColor {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
};

struct LedLayout {
  std::vector<LedPoint> points;  // in chain order
};

// A chain running clockwise around the frame edge from the top-left corner, with the given number of
// LEDs on each side, `inset` in from the edge (as a fraction of the frame, 0 to 0.25).
LedLayout perimeter_layout(int top, int right, int bottom, int left, float inset);

// The colour under each LED. `image` should already be a small downscale of the canvas (a few
// dozen pixels across); each LED takes the nearest pixel.
std::vector<LedColor> sample_leds(const Frame& image, const LedLayout& layout);

}  // namespace rm
