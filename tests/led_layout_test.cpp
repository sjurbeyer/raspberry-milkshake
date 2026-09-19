#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/led_layout.hpp"

using namespace rm;
using Catch::Matchers::WithinAbs;

TEST_CASE("a perimeter layout has the requested LEDs, clockwise from the top-left", "[led]") {
  const LedLayout layout = perimeter_layout(4, 3, 4, 3, 0.05f);
  REQUIRE(layout.points.size() == 14);

  // Top edge runs left to right along v = inset.
  for (int i = 0; i < 4; ++i) {
    REQUIRE_THAT(layout.points[static_cast<std::size_t>(i)].v, WithinAbs(0.05, 1e-6));
    if (i > 0) REQUIRE(layout.points[static_cast<std::size_t>(i)].u > layout.points[static_cast<std::size_t>(i - 1)].u);
  }
  // Right edge runs downwards along u = 1 - inset.
  for (int i = 4; i < 7; ++i) {
    REQUIRE_THAT(layout.points[static_cast<std::size_t>(i)].u, WithinAbs(0.95, 1e-6));
    if (i > 4) REQUIRE(layout.points[static_cast<std::size_t>(i)].v > layout.points[static_cast<std::size_t>(i - 1)].v);
  }
  // Bottom edge runs right to left, left edge runs upwards.
  REQUIRE(layout.points[8].u < layout.points[7].u);
  REQUIRE_THAT(layout.points[7].v, WithinAbs(0.95, 1e-6));
  REQUIRE_THAT(layout.points[11].u, WithinAbs(0.05, 1e-6));
  REQUIRE(layout.points[12].v < layout.points[11].v);
}

TEST_CASE("LEDs are spread evenly and stay inside the frame", "[led]") {
  const LedLayout layout = perimeter_layout(10, 6, 10, 6, 0.0f);
  for (const LedPoint& p : layout.points) {
    REQUIRE(p.u >= 0.0f);
    REQUIRE(p.u <= 1.0f);
    REQUIRE(p.v >= 0.0f);
    REQUIRE(p.v <= 1.0f);
  }
  // Half-step from the corner: 0.5 / 10 of the way across.
  REQUIRE_THAT(layout.points[0].u, WithinAbs(0.05, 1e-6));
  REQUIRE_THAT(layout.points[9].u, WithinAbs(0.95, 1e-6));
}

TEST_CASE("the inset is clamped", "[led]") {
  const LedLayout layout = perimeter_layout(1, 1, 1, 1, 0.9f);
  REQUIRE_THAT(layout.points[0].v, WithinAbs(0.25, 1e-6));
}

TEST_CASE("each LED takes the colour of the pixel under it", "[led]") {
  // 4x2 image: left half red, right half blue.
  Frame image;
  image.width = 4;
  image.height = 2;
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 4; ++x) {
      const bool left = x < 2;
      image.rgba.insert(image.rgba.end(), {static_cast<std::uint8_t>(left ? 255 : 0), 0,
                                           static_cast<std::uint8_t>(left ? 0 : 255), 255});
    }
  }
  LedLayout layout;
  layout.points = {{0.1f, 0.5f}, {0.9f, 0.5f}, {1.0f, 1.0f}, {-1.0f, -1.0f}};
  const auto colors = sample_leds(image, layout);
  REQUIRE(colors.size() == 4);
  REQUIRE(colors[0].r == 255);
  REQUIRE(colors[0].b == 0);
  REQUIRE(colors[1].r == 0);
  REQUIRE(colors[1].b == 255);
  REQUIRE(colors[2].b == 255);  // clamped to the last pixel
  REQUIRE(colors[3].r == 255);  // clamped to the first pixel
}

TEST_CASE("an empty image gives black LEDs", "[led]") {
  LedLayout layout;
  layout.points = {{0.5f, 0.5f}};
  const auto colors = sample_leds(Frame{}, layout);
  REQUIRE(colors.size() == 1);
  REQUIRE(colors[0].r == 0);
}
