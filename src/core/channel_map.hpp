#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace rm {

// Supported output layouts (the AVR is 5.1 at most).
enum class Layout { Stereo, Surround51 };

std::optional<Layout> layout_from_channels(int channels);  // 2 -> Stereo, 6 -> Surround51
int channel_count(Layout layout);

// Files use the WAVE/FLAC order FL FR FC LFE BL BR; ALSA HDMI expects FL FR RL RR FC LFE.
// Returns m with m[alsa_channel] = file_channel.
std::vector<int> file_to_alsa_map(Layout layout);

// Reorders interleaved float frames from file order to ALSA order. `in` and `out` must not overlap.
void remap_to_alsa(const float* in, float* out, std::size_t frames, Layout layout);

}  // namespace rm
