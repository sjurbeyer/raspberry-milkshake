#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "render/egl_context.hpp"
#include "render/png.hpp"
#include "render/projectm_host.hpp"

namespace rm::test {

inline const std::string kWavePreset = std::string(RM_SOURCE_DIR) + "/tests/fixtures/presets/test-wave.milk";
inline constexpr unsigned kPcmFrames = 512;  // what projectM keeps per render

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

inline std::vector<float> silence() { return std::vector<float>(kPcmFrames * 2, 0.0f); }

inline std::vector<float> sine(float amplitude, float frequency = 440.0f) {
  std::vector<float> pcm(kPcmFrames * 2);
  for (unsigned i = 0; i < kPcmFrames; ++i) {
    const float v = amplitude * std::sin(2.0f * 3.14159265f * frequency * static_cast<float>(i) / 48000.0f);
    pcm[i * 2] = v;
    pcm[i * 2 + 1] = v;
  }
  return pcm;
}

inline double mean_luma(const Frame& f) {
  double sum = 0;
  for (std::size_t i = 0; i < f.rgba.size(); i += 4) sum += (f.rgba[i] + f.rgba[i + 1] + f.rgba[i + 2]) / 3.0;
  return sum / static_cast<double>(f.rgba.size() / 4);
}

inline std::size_t bright_pixels(const Frame& f, int threshold) {
  std::size_t n = 0;
  for (std::size_t i = 0; i < f.rgba.size(); i += 4) {
    if ((f.rgba[i] + f.rgba[i + 1] + f.rgba[i + 2]) / 3 > threshold) ++n;
  }
  return n;
}

inline double mean_abs_diff(const Frame& a, const Frame& b) {
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

struct Rgb {
  int r, g, b;
};

inline Rgb pixel(const Frame& f, int x, int y) {
  const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(f.width) + static_cast<std::size_t>(x)) * 4;
  return {f.rgba[i], f.rgba[i + 1], f.rgba[i + 2]};
}

// Set RM_DUMP_FRAMES=<dir> to save snapshots while debugging.
inline void dump(const std::string& name, const Frame& f) {
  if (const char* dir = std::getenv("RM_DUMP_FRAMES")) {
    std::filesystem::create_directories(dir);
    write_png(std::string(dir) + "/" + name + ".png", f);
  }
}

}  // namespace rm::test
