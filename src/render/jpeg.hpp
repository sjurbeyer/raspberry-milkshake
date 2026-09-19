#pragma once

#include <cstdint>
#include <vector>

#include "core/frame.hpp"

namespace rm {

// Encodes the frame as a baseline JPEG (for the dashboard's MJPEG stream). Returns an empty vector
// on failure.
std::vector<std::uint8_t> encode_jpeg(const Frame& frame, int quality = 80);

}  // namespace rm
