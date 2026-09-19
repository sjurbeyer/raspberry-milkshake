#include "core/led_layout.hpp"

#include <algorithm>

namespace rm {

LedLayout perimeter_layout(int top, int right, int bottom, int left, float inset) {
  inset = std::clamp(inset, 0.0f, 0.25f);
  const float span = 1.0f - 2.0f * inset;
  LedLayout layout;
  auto along = [&](int count, int i) { return span * (static_cast<float>(i) + 0.5f) / static_cast<float>(count); };

  for (int i = 0; i < top; ++i) layout.points.push_back({inset + along(top, i), inset});
  for (int i = 0; i < right; ++i) layout.points.push_back({1.0f - inset, inset + along(right, i)});
  for (int i = 0; i < bottom; ++i) layout.points.push_back({1.0f - inset - along(bottom, i), 1.0f - inset});
  for (int i = 0; i < left; ++i) layout.points.push_back({inset, 1.0f - inset - along(left, i)});
  return layout;
}

std::vector<LedColor> sample_leds(const Frame& image, const LedLayout& layout) {
  std::vector<LedColor> colors(layout.points.size());
  if (image.width <= 0 || image.height <= 0) return colors;
  for (std::size_t i = 0; i < layout.points.size(); ++i) {
    const int x = std::clamp(static_cast<int>(layout.points[i].u * static_cast<float>(image.width)), 0, image.width - 1);
    const int y = std::clamp(static_cast<int>(layout.points[i].v * static_cast<float>(image.height)), 0, image.height - 1);
    const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)) * 4;
    colors[i] = {image.rgba[at], image.rgba[at + 1], image.rgba[at + 2]};
  }
  return colors;
}

}  // namespace rm
