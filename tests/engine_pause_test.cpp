#include "render_harness.hpp"

#include <catch2/catch_approx.hpp>

#include <atomic>
#include <filesystem>

#include "core/dir_catalog.hpp"
#include "render/downscaler.hpp"
#include "render/engine.hpp"

using namespace rm;
using namespace rm::test;

namespace {

constexpr SamplePos kSec = kSampleRate;
const std::filesystem::path kPresets = std::filesystem::path(RM_SOURCE_DIR) / "tests/fixtures/presets";

bool is_red(const Frame& f) {
  const Rgb corner = pixel(f, 2, 2);
  return corner.r > 200 && corner.g < 60 && corner.b < 60;
}

bool is_white(const Rgb& p) { return p.r > 200 && p.g > 200 && p.b > 200; }
bool is_dark(const Rgb& p) { return p.r < 12 && p.g < 12 && p.b < 12; }

Cue explicit_cue(SamplePos at, const char* preset) {
  Cue c;
  c.at = at;
  c.preset = preset;
  c.transition = Transition{Transition::Kind::Cut, 0.0};
  return c;
}

Track track_with(std::vector<Cue> cues) {
  Track t;
  t.file = "a.flac";
  t.grid.bpm = 120;
  t.cues = std::move(cues);
  return t;
}

// wave for the first second, then the red frame.
Collection wave_then_fill() {
  Collection c;
  c.title = "Test Album";
  c.tracks.push_back(track_with({explicit_cue(0, "test-wave.milk"), explicit_cue(1 * kSec, "test-fill.milk")}));
  return c;
}

Collection only_fill() {
  Collection c;
  c.title = "Red";
  c.tracks.push_back(track_with({explicit_cue(0, "test-fill.milk")}));
  return c;
}

EngineConfig config(PauseDisplay display = PauseDisplay::Hold) {
  EngineConfig c;
  c.fps = 60;
  c.preset_root = kPresets;
  c.pause_display = display;
  c.fade_frames = 10;
  c.pause_transition = Transition{Transition::Kind::Cut, 0.0};
  c.home_preset = "test-fill.milk";
  return c;
}

struct Rig {
  explicit Rig(EngineConfig c = config()) : engine(*harness.host, catalog, std::move(c)) {}
  void ticks(int n) {
    for (int i = 0; i < n; ++i) engine.tick();
  }
  Frame frame() { return harness.frame(); }

  Harness harness;
  DirCatalog catalog{kPresets};
  Engine engine;
};

}  // namespace

TEST_CASE("pausing freezes the clock, the cues and the picture", "[engine][pause]") {
  Rig rig;
  rig.engine.play("a", wave_then_fill());
  rig.ticks(30);
  const EngineStatus before = rig.engine.status();
  REQUIRE(before.mode == EngineMode::Playing);

  rig.engine.pause();
  rig.ticks(1);
  const EngineStatus paused = rig.engine.status();
  REQUIRE(paused.mode == EngineMode::Paused);
  REQUIRE(paused.title == "Test Album");
  REQUIRE(paused.preset == before.preset);
  REQUIRE(paused.media_seconds == before.media_seconds);

  const Frame held = rig.frame();
  rig.ticks(120);  // two seconds go by: the cue at 1.0 s must not fire
  REQUIRE(mean_abs_diff(held, rig.frame()) == 0.0);
  REQUIRE_FALSE(is_red(rig.frame()));
  REQUIRE(rig.engine.status().media_seconds == before.media_seconds);
}

TEST_CASE("continuing resumes from the same frame and the cue still lands on time", "[engine][pause]") {
  Rig rig;
  rig.engine.play("a", wave_then_fill());
  rig.ticks(30);
  const double at_pause = rig.engine.status().media_seconds;
  rig.engine.pause();
  rig.ticks(50);

  rig.engine.resume();
  rig.ticks(1);
  const EngineStatus resumed = rig.engine.status();
  REQUIRE(resumed.mode == EngineMode::Playing);
  REQUIRE(resumed.media_seconds == Catch::Approx(at_pause + 1.0 / 60.0).margin(1e-9));  // exactly one frame later

  rig.ticks(29);  // frames 31 to 59: still before 1.0 s
  REQUIRE_FALSE(is_red(rig.frame()));
  rig.ticks(1);   // frame 60
  REQUIRE(is_red(rig.frame()));
}

TEST_CASE("a track cannot end while paused", "[engine][pause]") {
  EngineConfig c = config();
  c.dev_track_length = 1 * kSec;
  Rig rig(c);
  std::atomic<int> finished{0};
  rig.engine.set_finished_callback([&] { ++finished; });

  rig.engine.play("a", wave_then_fill());
  rig.ticks(30);
  rig.engine.pause();
  rig.ticks(300);
  REQUIRE(finished == 0);
  REQUIRE(rig.engine.status().mode == EngineMode::Paused);

  rig.engine.resume();
  rig.ticks(31);  // frames 30 to 60, the end of the one-second track
  REQUIRE(finished == 1);
}

TEST_CASE("pause is ignored unless a collection is playing", "[engine][pause]") {
  Rig rig;
  rig.engine.pause();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);

  rig.engine.preview("p", wave_then_fill());
  rig.ticks(2);
  rig.engine.pause();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Previewing);

  rig.engine.resume();  // and resume without a pause does nothing
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Previewing);
}

TEST_CASE("playing something else, or stopping, ends the pause", "[engine][pause]") {
  Rig rig;
  rig.engine.play("a", wave_then_fill());
  rig.ticks(5);
  rig.engine.pause();
  rig.ticks(2);
  rig.engine.play("b", only_fill());
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
  REQUIRE(rig.engine.status().id == "b");
  REQUIRE(is_red(rig.frame()));

  rig.engine.pause();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Paused);
  rig.engine.stop();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);
}

TEST_CASE("previews are ignored while paused", "[engine][pause]") {
  Rig rig;
  rig.engine.play("a", wave_then_fill());
  rig.ticks(5);
  rig.engine.pause();
  rig.ticks(2);
  rig.engine.preview("b", only_fill());
  rig.engine.stop_preview();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Paused);
  REQUIRE(rig.engine.status().id == "a");
}

TEST_CASE("pausing an interactive collection holds its preset", "[engine][pause]") {
  Collection c;
  c.title = "Interactive";
  c.type = CollectionType::Interactive;
  c.preset_pool = PoolExpr{PoolExpr::Kind::All, {}};

  Rig rig;
  rig.engine.play("live", c);
  rig.ticks(5);
  const EngineStatus live = rig.engine.status();
  rig.engine.pause();
  rig.ticks(600);
  const EngineStatus held = rig.engine.status();
  REQUIRE(held.mode == EngineMode::Paused);
  REQUIRE(held.preset == live.preset);
  REQUIRE(held.media_seconds == live.media_seconds);
}

TEST_CASE("fade: the picture goes to black, the icon appears, and it fades back in", "[engine][pause][fade]") {
  Rig rig(config(PauseDisplay::Fade));
  rig.engine.play("a", only_fill());
  rig.ticks(5);
  const Frame live = rig.frame();
  REQUIRE(is_red(live));
  const double live_luma = mean_luma(live);

  rig.engine.pause();
  rig.ticks(1);  // one tenth of the way down
  rig.engine.draw_overlays();
  const Frame dimming = rig.frame();
  REQUIRE(mean_luma(dimming) < live_luma * 0.97);
  REQUIRE(mean_luma(dimming) > live_luma * 0.8);
  REQUIRE_FALSE(is_white(pixel(dimming, 151, 90)));  // no icon until the fade is done

  rig.ticks(9);  // fully faded
  REQUIRE(mean_luma(rig.frame()) < 1.0);

  // The LEDs sample before the icon is drawn, so they see black.
  Downscaler scaler(16, 9);
  const Frame small = scaler.capture(320, 180);
  for (int y = 0; y < small.height; ++y) {
    for (int x = 0; x < small.width; ++x) REQUIRE(is_dark(pixel(small, x, y)));
  }

  rig.engine.draw_overlays();
  const Frame with_icon = rig.frame();
  dump("pause-fade-dimming", dimming);
  dump("pause-fade-icon", with_icon);
  REQUIRE(is_white(pixel(with_icon, 151, 90)));   // left bar
  REQUIRE(is_white(pixel(with_icon, 169, 90)));   // right bar
  REQUIRE(is_dark(pixel(with_icon, 160, 90)));    // the gap between them
  REQUIRE(is_dark(pixel(with_icon, 10, 10)));

  rig.engine.resume();
  rig.ticks(1);
  rig.engine.draw_overlays();  // not paused any more: no icon
  const Frame veiled = rig.frame();
  REQUIRE(mean_luma(veiled) < 1.0);  // starts fully black
  REQUIRE_FALSE(is_white(pixel(veiled, 151, 90)));

  double previous = mean_luma(veiled);
  for (int i = 0; i < 5; ++i) {
    rig.ticks(1);
    const double now = mean_luma(rig.frame());
    REQUIRE(now > previous);  // getting brighter every frame
    previous = now;
  }
  rig.ticks(10);
  REQUIRE(is_red(rig.frame()));
  REQUIRE(mean_luma(rig.frame()) == Catch::Approx(live_luma).margin(2.0));
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
}

TEST_CASE("home: the output shows the home visualization and comes back", "[engine][pause][home]") {
  Rig rig(config(PauseDisplay::Home));
  rig.engine.play("a", wave_then_fill());
  rig.ticks(5);
  REQUIRE_FALSE(is_red(rig.frame()));  // the wave preset

  rig.engine.pause();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Paused);
  REQUIRE(is_red(rig.frame()));  // the configured home visualization (the red frame)

  rig.engine.resume();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
  REQUIRE_FALSE(is_red(rig.frame()));  // back to the wave preset that was showing
  REQUIRE(rig.engine.status().preset == "test-wave.milk");
}

TEST_CASE("home: re-planning the schedule while paused keeps the home visualization", "[engine][pause][home]") {
  Rig rig(config(PauseDisplay::Home));
  rig.engine.play("a", wave_then_fill());
  rig.ticks(5);
  rig.engine.pause();
  rig.ticks(2);
  rig.engine.refresh_schedule();  // for example after a preset was rated 0
  rig.ticks(2);
  REQUIRE(is_red(rig.frame()));
  REQUIRE(rig.engine.status().mode == EngineMode::Paused);
}

TEST_CASE("hold: nothing is drawn while paused, so the frame cannot change", "[engine][pause]") {
  Rig rig(config(PauseDisplay::Hold));
  rig.engine.play("a", only_fill());
  rig.ticks(5);
  rig.engine.pause();
  rig.ticks(1);
  const Frame first = rig.frame();
  rig.engine.draw_overlays();  // no icon in hold mode
  REQUIRE(mean_abs_diff(first, rig.frame()) == 0.0);
}
