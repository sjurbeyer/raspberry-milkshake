#pragma once

#include <cstdint>
#include <vector>

namespace rm {

// An RGBA8 image, top row first.
struct Frame {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;
};

}  // namespace rm
