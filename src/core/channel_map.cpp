#include "core/channel_map.hpp"

namespace rm {

std::optional<Layout> layout_from_channels(int channels) {
  if (channels == 2) return Layout::Stereo;
  if (channels == 6) return Layout::Surround51;
  return std::nullopt;
}

int channel_count(Layout layout) { return layout == Layout::Stereo ? 2 : 6; }

std::vector<int> file_to_alsa_map(Layout layout) {
  if (layout == Layout::Stereo) return {0, 1};
  return {0, 1, 4, 5, 2, 3};
}

void remap_to_alsa(const float* in, float* out, std::size_t frames, Layout layout) {
  const std::vector<int> map = file_to_alsa_map(layout);
  const std::size_t channels = map.size();
  for (std::size_t f = 0; f < frames; ++f) {
    for (std::size_t c = 0; c < channels; ++c) {
      out[f * channels + c] = in[f * channels + static_cast<std::size_t>(map[c])];
    }
  }
}

}  // namespace rm
