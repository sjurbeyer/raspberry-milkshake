#include "render_harness.hpp"

#include <QCoreApplication>

#include "app/engine_bridge.hpp"
#include "core/dir_catalog.hpp"
#include "render/engine.hpp"
#include "ui/app_controller.hpp"
#include "ui/library_model.hpp"

using namespace rm;
using namespace rm::test;

namespace {

QCoreApplication& app() {
  static int argc = 1;
  static char arg0[] = "app_bridge_pause_test";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication instance(argc, argv);
  return instance;
}

const std::filesystem::path kPresetRoot = std::filesystem::path(RM_SOURCE_DIR) / "tests/fixtures/presets";

struct Rig {
  Rig() : engine(*harness.host, catalog, config()) {
    app();
    model.load(QString(RM_SOURCE_DIR) + "/examples/collections");
    controller = std::make_unique<AppController>(&model);
    bridge = std::make_unique<EngineBridge>(controller.get(), &model, &engine);
  }
  static EngineConfig config() {
    EngineConfig c;
    c.fps = 60;
    c.preset_root = kPresetRoot;
    c.dev_track_length = kSampleRate / 2;
    return c;
  }
  void ticks(int n) {
    for (int i = 0; i < n; ++i) engine.tick();
    QCoreApplication::processEvents();
  }

  Harness harness;
  DirCatalog catalog{kPresetRoot};
  Engine engine;
  LibraryModel model;
  std::unique_ptr<AppController> controller;
  std::unique_ptr<EngineBridge> bridge;
};

}  // namespace

TEST_CASE("pausing from the controller freezes the engine, and continuing resumes it", "[app][pause]") {
  Rig rig;
  rig.controller->playNow("redline");
  rig.ticks(10);
  const double at_pause = rig.engine.status().media_seconds;

  rig.controller->pause();
  rig.ticks(60);
  const EngineStatus paused = rig.engine.status();
  REQUIRE(paused.mode == EngineMode::Paused);
  REQUIRE(paused.media_seconds == at_pause);

  rig.controller->resume();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
  REQUIRE(rig.engine.status().media_seconds > at_pause);
}

TEST_CASE("a collection paused mid-way does not end and skip to the next", "[app][pause]") {
  Rig rig;
  rig.controller->playNow("redline");  // its one track is 30 frames long here
  rig.controller->addToQueue("glass-garden");
  rig.ticks(10);
  rig.controller->pause();
  rig.ticks(200);
  REQUIRE(rig.controller->nowPlayingId() == "redline");  // the end never arrived
  REQUIRE(rig.engine.status().id == "redline");

  rig.controller->resume();
  rig.ticks(30);
  REQUIRE(rig.controller->nowPlayingId() == "glass-garden");  // now it does, and the queue advances
}

TEST_CASE("next while paused starts the next queued collection playing", "[app][pause]") {
  Rig rig;
  rig.controller->playNow("redline");
  rig.controller->addToQueue("glass-garden");
  rig.ticks(5);
  rig.controller->pause();
  rig.ticks(2);
  rig.controller->next();
  rig.ticks(2);
  const EngineStatus s = rig.engine.status();
  REQUIRE(s.mode == EngineMode::Playing);
  REQUIRE(s.id == "glass-garden");
}

TEST_CASE("stop and clear returns the main output to idle and empties the queue", "[app][pause]") {
  Rig rig;
  rig.controller->playNow("redline");
  rig.controller->addToQueue("glass-garden");
  rig.controller->addToQueue("after-hours");
  rig.ticks(5);
  rig.controller->pause();
  rig.ticks(2);

  rig.controller->stopAndClear();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);
  REQUIRE_FALSE(rig.controller->playing());
  REQUIRE(rig.controller->queueLength() == 0);

  rig.ticks(120);  // nothing in the queue comes back later
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);
}
