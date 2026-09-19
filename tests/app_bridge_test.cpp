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
  static char arg0[] = "app_bridge_test";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication instance(argc, argv);
  return instance;
}

const std::filesystem::path kPresetRoot = std::filesystem::path(RM_SOURCE_DIR) / "tests/fixtures/presets";

// The real controller and library over a real engine, ticked by hand.
struct Rig {
  Rig() : Rig(short_tracks()) {}
  explicit Rig(EngineConfig config) : engine(*harness.host, catalog, std::move(config)) {
    app();
    model.load(QString(RM_SOURCE_DIR) + "/examples/collections");
    controller = std::make_unique<AppController>(&model);
    bridge = std::make_unique<EngineBridge>(controller.get(), &model, &engine);
  }

  static EngineConfig short_tracks() {
    EngineConfig c;
    c.fps = 60;
    c.preset_root = kPresetRoot;
    c.dev_track_length = kSampleRate / 2;  // 0.5 s = 30 frames per track
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

TEST_CASE("browsing while idle previews the collection on the main output", "[app]") {
  Rig rig;
  rig.controller->browsing("redline");
  rig.ticks(2);
  const EngineStatus s = rig.engine.status();
  REQUIRE(s.mode == EngineMode::Previewing);
  REQUIRE(s.id == "redline");
  REQUIRE(s.title == "Redline");
}

TEST_CASE("leaving the swipe view ends the preview", "[app]") {
  Rig rig;
  rig.controller->browsing("redline");
  rig.ticks(2);
  rig.controller->stopBrowsing();
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);
}

TEST_CASE("play starts the collection and browsing while it plays changes nothing", "[app]") {
  Rig rig;
  rig.controller->browsing("redline");
  rig.ticks(2);
  rig.controller->playNow("glass-garden");
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
  REQUIRE(rig.engine.status().id == "glass-garden");

  rig.controller->browsing("redline");  // not allowed while playing: no preview request
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
  REQUIRE(rig.engine.status().id == "glass-garden");
}

TEST_CASE("play now interrupts what is playing", "[app]") {
  Rig rig;
  rig.controller->playNow("glass-garden");
  rig.ticks(2);
  rig.controller->playNow("redline");
  rig.ticks(2);
  REQUIRE(rig.engine.status().id == "redline");
  REQUIRE(rig.controller->nowPlayingId() == "redline");
}

TEST_CASE("next with nothing queued stops the main output", "[app]") {
  Rig rig;
  rig.controller->playNow("redline");
  rig.ticks(2);
  rig.controller->next();
  rig.ticks(2);
  REQUIRE_FALSE(rig.controller->playing());
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);
}

TEST_CASE("when a collection ends the next queued one starts by itself", "[app]") {
  Rig rig;
  rig.controller->playNow("redline");
  rig.controller->addToQueue("glass-garden");
  rig.ticks(2);
  REQUIRE(rig.engine.status().id == "redline");

  rig.ticks(30);  // redline's only track (0.5 s) ends; the callback hops to the UI thread
  REQUIRE(rig.controller->nowPlayingId() == "glass-garden");
  rig.ticks(2);   // and the resulting play command reaches the engine
  REQUIRE(rig.engine.status().mode == EngineMode::Playing);
  REQUIRE(rig.engine.status().id == "glass-garden");
  REQUIRE(rig.controller->queueLength() == 0);
}

TEST_CASE("when the last collection ends the controller and engine go idle", "[app]") {
  Rig rig;
  rig.controller->playNow("redline");
  rig.ticks(35);  // ends after ~31 frames; the controller then finds nothing queued
  REQUIRE_FALSE(rig.controller->playing());
  rig.ticks(20);
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);
}

TEST_CASE("the interactive collection plays from its pool", "[app]") {
  Rig rig;
  rig.controller->playNow("interactive");
  rig.ticks(3);
  const EngineStatus s = rig.engine.status();
  REQUIRE(s.mode == EngineMode::Playing);
  REQUIRE(s.title == "Interactive visualization");
  REQUIRE_FALSE(s.preset.empty());
}

TEST_CASE("unknown collections do nothing", "[app]") {
  Rig rig;
  rig.controller->playNow("no-such-collection");
  rig.ticks(2);
  REQUIRE(rig.engine.status().mode == EngineMode::Idle);
}
