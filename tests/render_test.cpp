#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "render/egl_context.hpp"
#include "render/png.hpp"
#include "render/projectm_host.hpp"

using namespace rm;

namespace {

const std::string kTestPreset = std::string(RM_SOURCE_DIR) + "/tests/fixtures/presets/test-wave.milk";
constexpr Transition kCut{Transition::Kind::Cut, 0.0};
constexpr unsigned kPcmFrames = 512;  // what projectM keeps per render

// One headless GLES context plus one projectM host, torn down in the right order.
struct Harness {
  std::unique_ptr<EglContext> egl;
  std::unique_ptr<ProjectMHost> host;

  explicit Harness(HostConfig config = HostConfig{320, 180, 32, 24, 60}) {
    std::string error;
    egl = EglContext::create_headless(config.width, config.height, error);
    INFO("EGL: " << error);
    REQUIRE(egl);
    host = ProjectMHost::create(config, error);
    INFO("projectM host: " << error);
    REQUIRE(host);
  }
  ~Harness() {
    host.reset();  // needs the context alive
    egl.reset();
  }

  void step(const std::vector<float>& pcm) {
    host->add_pcm_stereo(pcm.data(), static_cast<unsigned>(pcm.size() / 2));
    host->render();
  }
  Frame frame() const { return host->read_frame(); }
};

std::vector<float> silence() { return std::vector<float>(kPcmFrames * 2, 0.0f); }

std::vector<float> sine(float amplitude, float frequency = 440.0f) {
  std::vector<float> pcm(kPcmFrames * 2);
  for (unsigned i = 0; i < kPcmFrames; ++i) {
    const float v = amplitude * std::sin(2.0f * 3.14159265f * frequency * static_cast<float>(i) / 48000.0f);
    pcm[i * 2] = v;
    pcm[i * 2 + 1] = v;
  }
  return pcm;
}

double mean_luma(const Frame& f) {
  double sum = 0;
  for (std::size_t i = 0; i < f.rgba.size(); i += 4) sum += (f.rgba[i] + f.rgba[i + 1] + f.rgba[i + 2]) / 3.0;
  return sum / static_cast<double>(f.rgba.size() / 4);
}

std::size_t bright_pixels(const Frame& f, int threshold) {
  std::size_t n = 0;
  for (std::size_t i = 0; i < f.rgba.size(); i += 4) {
    if ((f.rgba[i] + f.rgba[i + 1] + f.rgba[i + 2]) / 3 > threshold) ++n;
  }
  return n;
}

double mean_abs_diff(const Frame& a, const Frame& b) {
  REQUIRE(a.rgba.size() == b.rgba.size());
  double sum = 0;
  std::size_t count = 0;
  for (std::size_t i = 0; i < a.rgba.size(); i += 4) {
    for (int c = 0; c < 3; ++c) {
      sum += std::abs(static_cast<int>(a.rgba[i + c]) - static_cast<int>(b.rgba[i + c]));
      ++count;
    }
  }
  return sum / static_cast<double>(count);
}

// Set RM_DUMP_FRAMES=<dir> to save snapshots while debugging.
void dump(const std::string& name, const Frame& f) {
  if (const char* dir = std::getenv("RM_DUMP_FRAMES")) {
    std::filesystem::create_directories(dir);
    write_png(std::string(dir) + "/" + name + ".png", f);
  }
}

}  // namespace

TEST_CASE("a headless GLES context and a projectM instance can be created", "[render]") {
  Harness h;
  INFO("renderer: " << h.egl->renderer() << ", version: " << h.egl->version());
  REQUIRE_FALSE(h.egl->renderer().empty());
  h.step(silence());
  const Frame f = h.frame();
  REQUIRE(f.width == 320);
  REQUIRE(f.height == 180);
  REQUIRE(f.rgba.size() == 320u * 180u * 4u);
}

TEST_CASE("loading a missing preset reports an error and rendering continues", "[render]") {
  Harness h;
  std::string error;
  REQUIRE_FALSE(h.host->load_preset("/nonexistent/preset.milk", kCut, error));
  REQUIRE_FALSE(error.empty());
  h.step(silence());
  REQUIRE(h.frame().rgba.size() == 320u * 180u * 4u);
}

TEST_CASE("the test preset loads", "[render]") {
  Harness h;
  std::string error;
  INFO(error);
  REQUIRE(h.host->load_preset(kTestPreset, kCut, error));
  REQUIRE(error.empty());
}

// Measured on llvmpipe at 320x180 with test-wave.milk: quiet ~980 bright pixels, loud ~1830,
// mean |diff| ~7.7, and ~0.015 between two silent frames. Thresholds leave wide margins.
TEST_CASE("the test preset draws a visible waveform in silence", "[render]") {
  Harness h;
  std::string error;
  REQUIRE(h.host->load_preset(kTestPreset, kCut, error));
  for (int i = 0; i < 3; ++i) h.step(silence());
  const Frame f = h.frame();
  dump("silence", f);
  REQUIRE(bright_pixels(f, 32) > 300);
  REQUIRE(bright_pixels(f, 32) < 5000);  // a thin ring, not a flooded frame
  REQUIRE(mean_luma(f) < 30.0);
}

TEST_CASE("the picture follows the audio", "[render]") {
  Harness h;
  std::string error;
  REQUIRE(h.host->load_preset(kTestPreset, kCut, error));
  for (int i = 0; i < 3; ++i) h.step(silence());
  const Frame quiet = h.frame();
  h.step(sine(0.9f));
  const Frame loud = h.frame();
  dump("follows-quiet", quiet);
  dump("follows-loud", loud);

  REQUIRE(mean_abs_diff(quiet, loud) > 3.0);
  REQUIRE(bright_pixels(loud, 32) > bright_pixels(quiet, 32) * 13 / 10);
}

TEST_CASE("audio reaches the very next rendered frame and nothing lingers", "[render]") {
  // The preset has no feedback (decay 0), so a frame shows exactly the audio added just before it.
  // This pins the zero-frame-latency property of the host: add_pcm_stereo() then render().
  Harness h;
  std::string error;
  REQUIRE(h.host->load_preset(kTestPreset, kCut, error));
  for (int i = 0; i < 3; ++i) h.step(silence());
  const Frame before = h.frame();
  h.step(sine(0.9f));
  const Frame during = h.frame();
  h.step(silence());
  const Frame after = h.frame();

  REQUIRE(mean_abs_diff(before, during) > 3.0);
  REQUIRE(mean_abs_diff(before, after) < 0.5);
}

TEST_CASE("a blended preset switch keeps rendering", "[render]") {
  Harness h;
  std::string error;
  REQUIRE(h.host->load_preset(kTestPreset, kCut, error));
  h.step(sine(0.5f));
  REQUIRE(h.host->load_preset(kTestPreset, Transition{Transition::Kind::Blend, 0.25}, error));
  Frame last;
  for (int i = 0; i < 20; ++i) {
    h.step(sine(0.5f));
    last = h.frame();
  }
  REQUIRE(bright_pixels(last, 32) > 300);
}

TEST_CASE("calibration: print frame statistics", "[.stats]") {
  Harness h;
  std::string error;
  REQUIRE(h.host->load_preset(kTestPreset, kCut, error));
  for (int i = 0; i < 3; ++i) h.step(silence());
  const Frame quiet = h.frame();
  dump("quiet", quiet);
  h.step(sine(0.9f));
  const Frame loud = h.frame();
  dump("loud", loud);
  h.step(silence());
  const Frame quiet_again = h.frame();
  dump("quiet_again", quiet_again);

  std::cout << "quiet:  luma=" << mean_luma(quiet) << " bright=" << bright_pixels(quiet, 32) << "\n"
            << "loud:   luma=" << mean_luma(loud) << " bright=" << bright_pixels(loud, 32) << "\n"
            << "quiet2: luma=" << mean_luma(quiet_again) << " bright=" << bright_pixels(quiet_again, 32) << "\n"
            << "diff(quiet,loud)=" << mean_abs_diff(quiet, loud) << " diff(quiet,quiet2)="
            << mean_abs_diff(quiet, quiet_again) << "\n";
}
