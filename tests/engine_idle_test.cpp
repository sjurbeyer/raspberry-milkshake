#include "render_harness.hpp"

#include <filesystem>

#include "core/dir_catalog.hpp"
#include "render/engine.hpp"

using namespace rm;
using namespace rm::test;

namespace {

const std::filesystem::path kPresets = std::filesystem::path(RM_SOURCE_DIR) / "tests/fixtures/presets";

bool is_red(const Frame& f) {
  const Rgb corner = pixel(f, 2, 2);
  return corner.r > 200 && corner.g < 60 && corner.b < 60;
}

Collection only_fill() {
  Cue cue;
  cue.at = 0;
  cue.preset = "test-fill.milk";
  cue.transition = Transition{Transition::Kind::Cut, 0.0};
  Track t;
  t.file = "a.flac";
  t.grid.bpm = 120;
  t.cues = {cue};
  Collection c;
  c.title = "Red";
  c.tracks.push_back(t);
  return c;
}

EngineConfig config() {
  EngineConfig c;
  c.fps = 60;
  c.preset_root = kPresets;
  return c;
}

}  // namespace

TEST_CASE("projectM's idle preset loads", "[render][idle]") {
  Harness h;
  std::string error;
  INFO(error);
  REQUIRE(h.host->load_preset(kIdlePresetUrl, Transition{Transition::Kind::Cut, 0.0}, error));
  for (int i = 0; i < 20; ++i) h.step(silence());
  REQUIRE(bright_pixels(h.frame(), 32) > 50);  // it draws something
}

TEST_CASE("stopping shows the idle visual, not the last preset", "[engine][idle]") {
  Harness harness;
  DirCatalog catalog(kPresets);
  Engine engine(*harness.host, catalog, config());

  engine.play("a", only_fill());
  for (int i = 0; i < 5; ++i) engine.tick();
  REQUIRE(is_red(harness.frame()));

  engine.stop();
  for (int i = 0; i < 10; ++i) engine.tick();
  REQUIRE(engine.status().mode == EngineMode::Idle);
  REQUIRE_FALSE(is_red(harness.frame()));  // the red preset is gone
  REQUIRE(bright_pixels(harness.frame(), 32) > 50);  // and the idle visual is on screen
}

TEST_CASE("the home pause display shows projectM's idle visual by default", "[engine][idle][pause]") {
  Harness harness;
  DirCatalog catalog(kPresets);
  EngineConfig c = config();
  c.pause_display = PauseDisplay::Home;
  c.pause_transition = Transition{Transition::Kind::Cut, 0.0};
  Engine engine(*harness.host, catalog, c);

  engine.play("a", only_fill());
  for (int i = 0; i < 5; ++i) engine.tick();
  REQUIRE(is_red(harness.frame()));

  engine.pause();
  for (int i = 0; i < 10; ++i) engine.tick();
  REQUIRE(engine.status().mode == EngineMode::Paused);
  REQUIRE_FALSE(is_red(harness.frame()));

  engine.resume();
  for (int i = 0; i < 3; ++i) engine.tick();
  REQUIRE(is_red(harness.frame()));  // back to the show
}
