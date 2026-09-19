#include "render_harness.hpp"

#include <algorithm>
#include <utility>

#include "core/sequencer.hpp"
#include "render/show_runner.hpp"

using namespace rm;
using namespace rm::test;

namespace {

constexpr SamplePos kSec = kSampleRate;
constexpr int kFps = 60;
const std::filesystem::path kPresetRoot = std::filesystem::path(RM_SOURCE_DIR) / "tests/fixtures/presets";

class NoCatalog final : public IPresetCatalog {
 public:
  std::vector<std::string> resolve(const PoolExpr&) const override { return {}; }
};

class SilentSource final : public IPcmSource {
 public:
  void read_stereo(SamplePos, unsigned frames, float* out) const override {
    std::fill(out, out + static_cast<std::size_t>(frames) * 2, 0.0f);
  }
};

// Remembers every window the runner asked for.
class RecordingSource final : public IPcmSource {
 public:
  mutable std::vector<std::pair<SamplePos, unsigned>> requests;
  void read_stereo(SamplePos start, unsigned frames, float* out) const override {
    requests.emplace_back(start, frames);
    std::fill(out, out + static_cast<std::size_t>(frames) * 2, 0.0f);
  }
};

// wave -> fill (red frame) at 1.0 s, both hard cuts.
Collection two_preset_show() {
  Collection c;
  Track t;
  t.file = "a.flac";
  Cue a;
  a.at = 0;
  a.preset = "test-wave.milk";
  a.transition = Transition{Transition::Kind::Cut, 0.0};
  Cue b;
  b.at = 1 * kSec;
  b.preset = "test-fill.milk";
  b.transition = Transition{Transition::Kind::Cut, 0.0};
  t.cues = {a, b};
  c.tracks.push_back(t);
  return c;
}

bool is_red(const Frame& f) {
  const Rgb corner = pixel(f, 2, 2);
  return corner.r > 200 && corner.g < 60 && corner.b < 60;
}

// First frame index (of 0..frames-1) whose corner is red, or -1.
int first_red_frame(ShowRunner& runner, Harness& h, int frames) {
  for (int i = 0; i < frames; ++i) {
    runner.step();
    if (is_red(h.frame())) return i;
  }
  return -1;
}

}  // namespace

TEST_CASE("a cue lands on the frame whose audio position reaches it", "[runner]") {
  Harness h;
  const Collection show = two_preset_show();
  Sequencer sequencer(show, 0, 3 * kSec, NoCatalog{});
  SilentSource audio;
  ShowRunner runner(*h.host, sequencer, audio, RunnerConfig{kFps, 0, 512, kPresetRoot});

  // Frame N covers position N * 800 samples, so the cue at 1.0 s (48000) is applied at frame 60.
  REQUIRE(first_red_frame(runner, h, 120) == 60);
  REQUIRE(runner.errors().empty());
  REQUIRE(runner.current_preset() == "test-fill.milk");
}

TEST_CASE("the frame before the cue is still the first preset", "[runner]") {
  Harness h;
  const Collection show = two_preset_show();
  Sequencer sequencer(show, 0, 3 * kSec, NoCatalog{});
  SilentSource audio;
  ShowRunner runner(*h.host, sequencer, audio, RunnerConfig{kFps, 0, 512, kPresetRoot});

  for (int i = 0; i < 59; ++i) runner.step();
  const Frame before = h.frame();
  dump("runner-before-cue", before);
  REQUIRE_FALSE(is_red(before));
  REQUIRE(bright_pixels(before, 32) > 300);  // the wave preset's ring
}

TEST_CASE("a visual lead makes the cue fire earlier by exactly that many frames", "[runner]") {
  Harness h;
  const Collection show = two_preset_show();
  Sequencer sequencer(show, 0, 3 * kSec, NoCatalog{});
  SilentSource audio;
  const SamplePos three_frames = 3 * kSec / kFps;
  ShowRunner runner(*h.host, sequencer, audio, RunnerConfig{kFps, three_frames, 512, kPresetRoot});
  REQUIRE(first_red_frame(runner, h, 120) == 57);
}

TEST_CASE("the audio window ends at the frame's position, lead included", "[runner]") {
  Harness h;
  const Collection show = two_preset_show();
  Sequencer sequencer(show, 0, 3 * kSec, NoCatalog{});
  RecordingSource audio;
  ShowRunner runner(*h.host, sequencer, audio, RunnerConfig{kFps, 1000, 512, kPresetRoot});

  for (int i = 0; i < 5; ++i) {
    const auto result = runner.step();
    REQUIRE(result.frame == i);
    REQUIRE(result.media_pos == i * 800 + 1000);
  }
  REQUIRE(audio.requests.size() == 5);
  for (int i = 0; i < 5; ++i) {
    REQUIRE(audio.requests[static_cast<std::size_t>(i)].first + 512 == i * 800 + 1000);
    REQUIRE(audio.requests[static_cast<std::size_t>(i)].second == 512u);
  }
}

TEST_CASE("seek loads the preset that is active at the new position", "[runner]") {
  Harness h;
  const Collection show = two_preset_show();
  Sequencer sequencer(show, 0, 3 * kSec, NoCatalog{});
  SilentSource audio;
  ShowRunner runner(*h.host, sequencer, audio, RunnerConfig{kFps, 0, 512, kPresetRoot});

  for (int i = 0; i < 90; ++i) runner.step();  // well past the cue
  REQUIRE(is_red(h.frame()));

  runner.seek(kSec / 2);  // back before the cue
  const auto result = runner.step();
  REQUIRE(result.frame == 30);
  REQUIRE_FALSE(is_red(h.frame()));

  runner.seek(2 * kSec);  // forward again
  runner.step();
  REQUIRE(is_red(h.frame()));
}

TEST_CASE("a preset that fails to load is reported and the show goes on", "[runner]") {
  Harness h;
  Collection show = two_preset_show();
  show.tracks[0].cues[1].preset = "does-not-exist.milk";
  Sequencer sequencer(show, 0, 3 * kSec, NoCatalog{});
  SilentSource audio;
  ShowRunner runner(*h.host, sequencer, audio, RunnerConfig{kFps, 0, 512, kPresetRoot});

  for (int i = 0; i < 90; ++i) runner.step();
  REQUIRE(runner.errors().size() == 1);
  REQUIRE(runner.errors()[0].find("does-not-exist.milk") != std::string::npos);
  REQUIRE_FALSE(is_red(h.frame()));  // still the previous preset
}
