#include "render_harness.hpp"

#include <atomic>
#include <thread>

#include "core/dir_catalog.hpp"
#include "render/engine.hpp"

using namespace rm;
using namespace rm::test;

namespace {

constexpr SamplePos kSec = kSampleRate;
const std::filesystem::path kPresetRoot = std::filesystem::path(RM_SOURCE_DIR) / "tests/fixtures/presets";

bool is_red(const Frame& f) {
  const Rgb corner = pixel(f, 2, 2);
  return corner.r > 200 && corner.g < 60 && corner.b < 60;
}

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
Collection wave_then_fill(int tracks = 1) {
  Collection c;
  c.title = "Test Album";
  for (int i = 0; i < tracks; ++i) c.tracks.push_back(track_with({explicit_cue(0, "test-wave.milk"), explicit_cue(1 * kSec, "test-fill.milk")}));
  return c;
}

struct Rig {
  Harness harness;
  DirCatalog catalog{kPresetRoot};
  Engine engine;

  explicit Rig(EngineConfig config = default_config()) : engine(*harness.host, catalog, std::move(config)) {}

  static EngineConfig default_config() {
    EngineConfig c;
    c.fps = 60;
    c.preset_root = kPresetRoot;
    return c;
  }
  void ticks(int n) {
    for (int i = 0; i < n; ++i) engine.tick();
  }
  Frame frame() { return harness.frame(); }
};

}  // namespace

TEST_CASE("the engine starts idle and keeps rendering", "[engine]") {
  Rig rig;
  rig.ticks(3);
  const EngineStatus s = rig.engine.status();
  REQUIRE(s.mode == EngineMode::Idle);
  REQUIRE(s.id.empty());
  REQUIRE(rig.frame().rgba.size() == 320u * 180u * 4u);
}

TEST_CASE("play runs the collection's cues on the frame their time is reached", "[engine]") {
  Rig rig;
  rig.engine.play("night", wave_then_fill());
  rig.ticks(60);  // frames 0..59: still the wave preset
  REQUIRE_FALSE(is_red(rig.frame()));
  rig.ticks(1);   // frame 60 is at 1.0 s
  REQUIRE(is_red(rig.frame()));

  const EngineStatus s = rig.engine.status();
  REQUIRE(s.mode == EngineMode::Playing);
  REQUIRE(s.id == "night");
  REQUIRE(s.title == "Test Album");
  REQUIRE(s.track == 1);
  REQUIRE(s.track_count == 1);
  REQUIRE(s.preset == "test-fill.milk");
  REQUIRE(s.media_seconds == 1.0);
}

TEST_CASE("tracks play in order, then the collection ends and reports it once", "[engine]") {
  EngineConfig config = Rig::default_config();
  config.dev_track_length = 1 * kSec;
  Rig rig(config);
  std::atomic<int> finished{0};
  rig.engine.set_finished_callback([&] { ++finished; });

  rig.engine.play("two", wave_then_fill(2));
  rig.ticks(60);
  REQUIRE(rig.engine.status().track == 1);
  rig.ticks(1);  // track 1 reached its end
  REQUIRE(rig.engine.status().track == 2);
  REQUIRE(finished == 0);

  rig.ticks(60);  // track 2 starts its own clock on the next tick, so it is one frame short of its end
  REQUIRE(finished == 0);
  rig.ticks(1);
  REQUIRE(finished == 1);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);  // the grace period keeps the visuals up

  rig.ticks(config.finish_grace_frames);
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);
  REQUIRE(finished == 1);
}

TEST_CASE("a new play command during the grace period replaces the ending collection", "[engine]") {
  EngineConfig config = Rig::default_config();
  config.dev_track_length = 1 * kSec;
  Rig rig(config);
  rig.engine.play("first", wave_then_fill());
  rig.ticks(61);
  rig.engine.play("second", wave_then_fill());
  rig.ticks(1);
  const EngineStatus s = rig.engine.status();
  REQUIRE(s.mode == EngineMode::Playing);
  REQUIRE(s.id == "second");
  rig.ticks(config.finish_grace_frames + 5);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);  // it is not cut short by the old ending
}

TEST_CASE("a preview starts at its start time and loops inside its window", "[engine]") {
  Collection c;
  c.title = "Preview Me";
  c.tracks.push_back(track_with({explicit_cue(0, "test-fill.milk")}));
  Preview p;
  p.track = 1;
  p.start = kSec / 2;
  p.length = kSec;
  c.preview = p;

  Rig rig;
  rig.engine.preview("pm", c);
  rig.ticks(1);
  REQUIRE(rig.engine.status().mode == EngineMode::Previewing);
  REQUIRE(rig.engine.status().media_seconds >= 0.5);
  REQUIRE(is_red(rig.frame()));  // the cue before the window start is the one that is active

  double lowest = 10;
  double highest = 0;
  bool wrapped = false;
  for (int i = 0; i < 100; ++i) {
    rig.ticks(1);
    const double t = rig.engine.status().media_seconds;
    if (t > 1.4) highest = t;
    if (highest > 1.4 && t < 0.7) wrapped = true;
    lowest = std::min(lowest, t);
  }
  REQUIRE(lowest >= 0.5);
  REQUIRE(highest <= 1.5 + 0.03);
  REQUIRE(wrapped);
}

TEST_CASE("a preview with its own preset shows that preset", "[engine]") {
  Collection c;
  c.title = "Interactive";
  c.type = CollectionType::Interactive;
  c.preset_pool = PoolExpr{PoolExpr::Kind::All, {}};
  Preview p;
  p.preset = "test-fill.milk";
  c.preview = p;

  Rig rig;
  rig.engine.preview("live", c);
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Previewing);
  REQUIRE(rig.engine.status().preset == "test-fill.milk");
}

TEST_CASE("stopping a preview goes back to idle", "[engine]") {
  Rig rig;
  rig.engine.preview("pm", wave_then_fill());
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Previewing);
  rig.engine.stop_preview();
  rig.ticks(1);
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);
}

TEST_CASE("previews are ignored while something plays, and play interrupts a preview", "[engine]") {
  Rig rig;
  rig.engine.play("a", wave_then_fill());
  rig.ticks(2);
  rig.engine.preview("b", wave_then_fill());
  rig.engine.stop_preview();  // must not stop the playing collection either
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
  REQUIRE(rig.engine.status().id == "a");

  rig.engine.stop();
  rig.ticks(1);
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);

  rig.engine.preview("b", wave_then_fill());
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Previewing);
  rig.engine.play("c", wave_then_fill());
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
  REQUIRE(rig.engine.status().id == "c");
}

TEST_CASE("an interactive collection plays presets from its pool", "[engine]") {
  Collection c;
  c.title = "Interactive visualization";
  c.type = CollectionType::Interactive;
  c.preset_pool = PoolExpr{PoolExpr::Kind::All, {}};

  Rig rig;
  rig.engine.play("live", c);
  rig.ticks(3);
  const EngineStatus s = rig.engine.status();
  REQUIRE(s.mode == EngineMode::Playing);
  REQUIRE(s.track_count == 1);
  REQUIRE((s.preset == "test-wave.milk" || s.preset == "test-fill.milk"));
}

TEST_CASE("presets that fail to load are counted and the show goes on", "[engine]") {
  Collection c;
  c.title = "Broken";
  c.tracks.push_back(track_with({explicit_cue(0, "test-wave.milk"), explicit_cue(kSec / 2, "does-not-exist.milk")}));

  Rig rig;
  rig.engine.play("b", c);
  rig.ticks(40);
  const EngineStatus s = rig.engine.status();
  REQUIRE(s.mode == EngineMode::Playing);
  REQUIRE(s.error_count == 1);
  REQUIRE(s.last_error.find("does-not-exist.milk") != std::string::npos);
  REQUIRE(s.preset == "test-wave.milk");
}

TEST_CASE("commands can be posted from another thread", "[engine]") {
  Rig rig;
  std::thread poster([&] { rig.engine.play("threaded", wave_then_fill()); });
  poster.join();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
  REQUIRE(rig.engine.status().id == "threaded");
}
