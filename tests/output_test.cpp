#include "render_harness.hpp"

#include <algorithm>

#include "core/led_layout.hpp"
#include "render/downscaler.hpp"
#include "render/jpeg.hpp"

using namespace rm;
using namespace rm::test;

namespace {

constexpr Transition kCut{Transition::Kind::Cut, 0.0};
const std::string kFillPreset = std::string(RM_SOURCE_DIR) + "/tests/fixtures/presets/test-fill.milk";

// Averages 5x5 blocks of a 320x180 frame down to 64x36.
Frame box_downscale_5x(const Frame& f) {
  Frame out;
  out.width = f.width / 5;
  out.height = f.height / 5;
  out.rgba.resize(static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height) * 4, 255);
  for (int y = 0; y < out.height; ++y) {
    for (int x = 0; x < out.width; ++x) {
      int sum[3] = {0, 0, 0};
      for (int dy = 0; dy < 5; ++dy) {
        for (int dx = 0; dx < 5; ++dx) {
          const Rgb p = pixel(f, x * 5 + dx, y * 5 + dy);
          sum[0] += p.r;
          sum[1] += p.g;
          sum[2] += p.b;
        }
      }
      const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(out.width) + static_cast<std::size_t>(x)) * 4;
      for (int c = 0; c < 3; ++c) out.rgba[at + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(sum[c] / 25);
    }
  }
  return out;
}

Frame flipped(const Frame& f) {
  Frame out = f;
  const std::size_t stride = static_cast<std::size_t>(f.width) * 4;
  for (int y = 0; y < f.height; ++y) {
    std::copy(f.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) * stride),
              f.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y + 1) * stride),
              out.rgba.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(f.height - 1 - y) * stride));
  }
  return out;
}

}  // namespace

// test-fill.milk is a red frame around a black rectangle covering the middle half of each axis.
TEST_CASE("the downscaler keeps the layout of the picture at the requested size", "[output]") {
  Harness h;
  std::string error;
  REQUIRE(h.host->load_preset(kFillPreset, kCut, error));
  h.step(silence());

  dump("fill-full", h.frame());
  Downscaler scaler(8, 4);
  const Frame small = scaler.capture(320, 180);
  dump("fill-small", small);
  REQUIRE(small.width == 8);
  REQUIRE(small.height == 4);
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 8; ++x) {
      const bool centre = x >= 2 && x <= 5 && y >= 1 && y <= 2;
      const Rgb p = pixel(small, x, y);
      INFO("pixel " << x << "," << y << " = " << p.r << "," << p.g << "," << p.b);
      if (centre) {
        REQUIRE(p.r < 40);
      } else {
        REQUIRE(p.r > 200);
        REQUIRE(p.g < 60);
      }
    }
  }
}

TEST_CASE("the downscaled image matches the full frame and is not upside down", "[output]") {
  Harness h;
  std::string error;
  REQUIRE(h.host->load_preset(kWavePreset, kCut, error));
  for (int i = 0; i < 3; ++i) h.step(silence());
  h.step(sine(0.9f));  // the asymmetric two-lobed shape

  Downscaler scaler(64, 36);
  const Frame gpu = scaler.capture(320, 180);
  const Frame cpu = box_downscale_5x(h.frame());
  dump("downscaled-gpu", gpu);
  dump("downscaled-cpu", cpu);

  const double same = mean_abs_diff(gpu, cpu);
  const double upside_down = mean_abs_diff(gpu, flipped(cpu));
  INFO("same=" << same << " flipped=" << upside_down);
  REQUIRE(same < upside_down / 2.0);
}

TEST_CASE("LEDs pick up the colours of the picture without a second render", "[output]") {
  Harness h;
  std::string error;
  REQUIRE(h.host->load_preset(kFillPreset, kCut, error));
  h.step(silence());

  Downscaler scaler(64, 36);
  const auto colors = sample_leds(scaler.capture(320, 180), perimeter_layout(20, 12, 20, 12, 0.04f));
  REQUIRE(colors.size() == 64);
  for (const LedColor& c : colors) {
    REQUIRE(c.r > 200);
    REQUIRE(c.g < 60);
    REQUIRE(c.b < 60);
  }
}

TEST_CASE("frames encode as JPEG", "[output]") {
  Frame f;
  f.width = 64;
  f.height = 36;
  f.rgba.assign(64u * 36u * 4u, 200);
  const auto jpeg = encode_jpeg(f);
  REQUIRE(jpeg.size() > 100);
  REQUIRE(jpeg.size() < f.rgba.size());
  REQUIRE(jpeg[0] == 0xFF);
  REQUIRE(jpeg[1] == 0xD8);  // start of image
  REQUIRE(jpeg[jpeg.size() - 2] == 0xFF);
  REQUIRE(jpeg[jpeg.size() - 1] == 0xD9);  // end of image
  REQUIRE(encode_jpeg(Frame{}).empty());
}
