#pragma once

#include <string>

#include "render/projectm_host.hpp"

namespace rm {

// Writes the frame as an uncompressed RGBA PNG (debug snapshots and golden images; no zlib needed).
bool write_png(const std::string& path, const Frame& frame);

}  // namespace rm
