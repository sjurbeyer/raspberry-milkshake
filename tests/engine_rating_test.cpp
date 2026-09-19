#include "render_harness.hpp"

#include <filesystem>

#include "core/dir_catalog.hpp"
#include "render/engine.hpp"

using namespace rm;
using namespace rm::test;

namespace {

const std::filesystem::path kFixtures = std::filesystem::path(RM_SOURCE_DIR) / "tests/fixtures/presets";

// A private copy of the two test presets, so a test can rate them.
struct PresetCopy {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "rm_engine_rating_test";
  PresetCopy() {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    for (const char* name : {"test-wave.milk", "test-fill.milk"}) {
      std::filesystem::copy_file(kFixtures / name, root / name);
    }
  }
  ~PresetCopy() { std::filesystem::remove_all(root); }
};

Track track_with(std::vector<Cue> cues) {
  Track t;
  t.file = "a.flac";
  t.grid.bpm = 120;
  t.cues = std::move(cues);
  return t;
}

EngineConfig config_for(const std::filesystem::path& root) {
  EngineConfig c;
  c.fps = 60;
  c.preset_root = root;
  return c;
}

}  // namespace

TEST_CASE("refreshing the schedule drops a preset that was just rated 0 and carries on", "[engine][rating]") {
  const PresetCopy presets;
  Harness harness;
  DirCatalog catalog(presets.root);
  Engine engine(*harness.host, catalog, config_for(presets.root));

  Collection c;
  c.title = "Pool";
  Cue cue;
  cue.at = 0;
  cue.pool = PoolExpr{PoolExpr::Kind::All, {}};
  cue.transition = Transition{Transition::Kind::Cut, 0.0};
  c.tracks.push_back(track_with({cue}));

  engine.play("pool", c);
  for (int i = 0; i < 30; ++i) engine.tick();
  const EngineStatus before = engine.status();
  REQUIRE_FALSE(before.preset.empty());

  REQUIRE(catalog.set_rating(before.preset, 0));
  engine.refresh_schedule();
  engine.tick();
  const EngineStatus after = engine.status();
  REQUIRE_FALSE(after.preset.empty());
  REQUIRE(after.preset != before.preset);                // the hidden one is gone, the other takes over
  REQUIRE(after.media_seconds >= before.media_seconds);  // and the show did not restart
  REQUIRE(after.mode == EngineMode::Playing);
}

TEST_CASE("a preset named in a cue still plays when it is rated 0", "[engine][rating]") {
  const PresetCopy presets;
  Harness harness;
  DirCatalog catalog(presets.root);
  Engine engine(*harness.host, catalog, config_for(presets.root));

  Cue cue;
  cue.at = 0;
  cue.preset = "test-wave.milk";
  cue.transition = Transition{Transition::Kind::Cut, 0.0};
  Collection c;
  c.title = "Explicit";
  c.tracks.push_back(track_with({cue}));
  REQUIRE(catalog.set_rating("test-wave.milk", 0));

  engine.play("e", c);
  engine.tick();
  engine.refresh_schedule();
  engine.tick();
  REQUIRE(engine.status().preset == "test-wave.milk");
}
