#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include "ui/app_controller.hpp"
#include "ui/library_model.hpp"

using namespace rm;

namespace {

QCoreApplication& app() {
  static int argc = 1;
  static char arg0[] = "ui_pause_test";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication instance(argc, argv);
  return instance;
}

struct Fixture {
  Fixture() {
    app();
    model.load(QString(RM_SOURCE_DIR) + "/examples/collections");
    controller = std::make_unique<AppController>(&model);
  }
  LibraryModel model;
  std::unique_ptr<AppController> controller;
};

}  // namespace

TEST_CASE("pause and resume toggle the paused flag and tell the engine", "[ui][pause]") {
  Fixture f;
  QSignalSpy paused(f.controller.get(), &AppController::pauseRequested);
  QSignalSpy resumed(f.controller.get(), &AppController::resumeRequested);
  QSignalSpy changed(f.controller.get(), &AppController::transportChanged);

  f.controller->playNow("redline");
  changed.clear();
  REQUIRE_FALSE(f.controller->paused());

  f.controller->pause();
  REQUIRE(f.controller->paused());
  REQUIRE(f.controller->playing());  // there is still a current item
  REQUIRE(f.controller->nowPlayingId() == "redline");
  REQUIRE(paused.count() == 1);
  REQUIRE(changed.count() == 1);

  f.controller->resume();
  REQUIRE_FALSE(f.controller->paused());
  REQUIRE(resumed.count() == 1);
  REQUIRE(changed.count() == 2);
}

TEST_CASE("pause and resume are ignored when they cannot apply", "[ui][pause]") {
  Fixture f;
  QSignalSpy paused(f.controller.get(), &AppController::pauseRequested);
  QSignalSpy resumed(f.controller.get(), &AppController::resumeRequested);

  f.controller->pause();   // idle
  f.controller->resume();  // not paused
  REQUIRE(paused.count() == 0);
  REQUIRE(resumed.count() == 0);

  f.controller->playNow("redline");
  f.controller->pause();
  f.controller->pause();  // already paused
  REQUIRE(paused.count() == 1);
}

TEST_CASE("togglePause alternates", "[ui][pause]") {
  Fixture f;
  f.controller->playNow("redline");
  f.controller->togglePause();
  REQUIRE(f.controller->paused());
  f.controller->togglePause();
  REQUIRE_FALSE(f.controller->paused());
}

TEST_CASE("a paused collection still counts as playing for the browse rules", "[ui][pause]") {
  Fixture f;
  QSignalSpy previews(f.controller.get(), &AppController::previewRequested);
  f.controller->playNow("redline");
  f.controller->pause();

  const auto album = f.controller->browseActions("glass-garden");
  REQUIRE_FALSE(album["previewAllowed"].toBool());
  REQUIRE(album["addToQueue"].toBool());
  REQUIRE(album["playNow"].toBool());
  REQUIRE_FALSE(f.controller->browseActions("interactive")["addToQueue"].toBool());

  f.controller->browsing("glass-garden");
  REQUIRE(previews.count() == 0);
}

TEST_CASE("next and play now end a pause", "[ui][pause]") {
  Fixture f;
  QSignalSpy plays(f.controller.get(), &AppController::playRequested);
  f.controller->playNow("redline");
  f.controller->addToQueue("glass-garden");
  f.controller->pause();

  f.controller->next();
  REQUIRE_FALSE(f.controller->paused());
  REQUIRE(f.controller->nowPlayingId() == "glass-garden");
  REQUIRE(plays.last().at(0).toString() == "glass-garden");

  f.controller->pause();
  f.controller->playNow("after-hours");
  REQUIRE_FALSE(f.controller->paused());
  REQUIRE(f.controller->nowPlayingId() == "after-hours");
}

TEST_CASE("stopAndClear ends playback and empties the queue", "[ui][pause]") {
  Fixture f;
  QSignalSpy stops(f.controller.get(), &AppController::stopped);
  f.controller->playNow("redline");
  f.controller->addToQueue("glass-garden");
  f.controller->addToQueue("after-hours");
  REQUIRE(f.controller->queueLength() == 2);

  f.controller->stopAndClear();
  REQUIRE_FALSE(f.controller->playing());
  REQUIRE_FALSE(f.controller->paused());
  REQUIRE(f.controller->queueLength() == 0);
  REQUIRE(f.controller->upcomingTitles().isEmpty());
  REQUIRE(f.controller->nowPlayingId().isEmpty());
  REQUIRE(stops.count() == 1);

  f.controller->stopAndClear();  // nothing to stop
  REQUIRE(stops.count() == 1);
}

TEST_CASE("stopAndClear works while paused and previews are possible again", "[ui][pause]") {
  Fixture f;
  QSignalSpy previews(f.controller.get(), &AppController::previewRequested);
  f.controller->playNow("redline");
  f.controller->pause();
  f.controller->stopAndClear();
  REQUIRE_FALSE(f.controller->paused());

  f.controller->browsing("glass-garden");
  REQUIRE(previews.count() == 1);
}
