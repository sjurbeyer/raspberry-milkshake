#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include "ui/app_controller.hpp"
#include "ui/library_filter.hpp"
#include "ui/library_model.hpp"

using namespace rm;

namespace {

// Qt object machinery wants an application object to exist.
QCoreApplication& app() {
  static int argc = 1;
  static char arg0[] = "ui_model_test";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication instance(argc, argv);
  return instance;
}

const QString kLibrary = QString(RM_SOURCE_DIR) + "/examples/collections";

struct Fixture {
  Fixture() {
    app();
    model.load(kLibrary);
    filter.setSourceModel(&model);
  }
  LibraryModel model;
  LibraryFilter filter;
};

QStringList titles(const LibraryFilter& f) {
  QStringList out;
  for (int i = 0; i < f.rowCount(); ++i) out << f.titleAt(i);
  return out;
}

}  // namespace

TEST_CASE("the library lists every valid collection, interactive first then alphabetical", "[ui]") {
  Fixture f;
  REQUIRE(f.model.problems().isEmpty());
  REQUIRE(f.model.rowCount() == 9);
  const QStringList all = titles(f.filter);
  REQUIRE(all.first() == "Interactive visualization");
  REQUIRE(all.mid(1) == QStringList{"After Hours", "Club Set", "Deep Field", "Glass Garden", "Night Drive",
                                    "Redline", "Soft Static", "Sunday Mix"});
}

TEST_CASE("collection rows expose their data to QML", "[ui]") {
  Fixture f;
  const int row = f.filter.rowOf("night-drive");
  REQUIRE(row >= 0);
  const QModelIndex idx = f.filter.index(row, 0);
  REQUIRE(idx.data(LibraryModel::TypeRole).toString() == "album");
  REQUIRE(idx.data(LibraryModel::TagsRole).toStringList() == QStringList{"pleasing", "edm"});
  REQUIRE(idx.data(LibraryModel::TrackCountRole).toInt() == 2);
  REQUIRE(idx.data(LibraryModel::CoverRole).toUrl().isLocalFile());
  REQUIRE(idx.data(LibraryModel::CoverRole).toUrl().fileName() == "cover.svg");
  REQUIRE(f.model.roleNames().values().contains("collectionId"));
  REQUIRE_FALSE(f.model.roleNames().values().contains("id"));
}

TEST_CASE("the tag list comes from the collections", "[ui]") {
  Fixture f;
  REQUIRE(f.model.allTags() == QStringList{"challenging", "edm", "pleasing"});
}

TEST_CASE("the type filter narrows the grid", "[ui]") {
  Fixture f;
  f.filter.setTypeFilter("playlist");
  REQUIRE(titles(f.filter) == QStringList{"Club Set", "Sunday Mix"});
  f.filter.setTypeFilter("interactive");
  REQUIRE(titles(f.filter) == QStringList{"Interactive visualization"});
  f.filter.setTypeFilter("album");
  REQUIRE(f.filter.count() == 6);
  f.filter.setTypeFilter("all");
  REQUIRE(f.filter.count() == 9);
}

TEST_CASE("type and tag filters combine", "[ui]") {
  Fixture f;
  f.filter.setTagFilter("edm");
  REQUIRE(titles(f.filter) == QStringList{"After Hours", "Club Set", "Night Drive", "Redline"});
  f.filter.setTypeFilter("album");
  REQUIRE(titles(f.filter) == QStringList{"After Hours", "Night Drive", "Redline"});
  f.filter.setTagFilter("nonexistent");
  REQUIRE(f.filter.count() == 0);
}

TEST_CASE("changing a filter reports the new count", "[ui]") {
  Fixture f;
  QSignalSpy spy(&f.filter, &LibraryFilter::countChanged);
  f.filter.setTypeFilter("playlist");
  REQUIRE(spy.count() >= 1);
  REQUIRE(f.filter.rowOf("night-drive") == -1);
}

TEST_CASE("a broken collection is skipped and reported", "[ui]") {
  app();
  const QString root = QDir::temp().filePath("rm_ui_model_test");
  QDir(root).removeRecursively();
  QDir().mkpath(root + "/broken");
  QDir().mkpath(root + "/not-a-collection");
  QFile file(root + "/broken/collection.toml");
  REQUIRE(file.open(QIODevice::WriteOnly));
  file.write("format = 1\ntype = \"album\"\ncover = \"c.jpg\"\n[[track]]\nfile = \"a.flac\"\n");  // no title
  file.close();

  LibraryModel model;
  model.load(root);
  REQUIRE(model.rowCount() == 0);
  REQUIRE(model.problems().size() == 1);
  REQUIRE(model.problems()[0].startsWith("broken: "));
  QDir(root).removeRecursively();
}

TEST_CASE("browse actions follow the transport state", "[ui]") {
  Fixture f;
  AppController c(&f.model);

  auto idle = c.browseActions("night-drive");
  REQUIRE(idle["previewAllowed"].toBool());
  REQUIRE(idle["playNow"].toBool());
  REQUIRE_FALSE(idle["addToQueue"].toBool());

  c.playNow("night-drive");
  auto playing = c.browseActions("redline");
  REQUIRE_FALSE(playing["previewAllowed"].toBool());
  REQUIRE(playing["playNow"].toBool());
  REQUIRE(playing["addToQueue"].toBool());
  REQUIRE_FALSE(c.browseActions("interactive")["addToQueue"].toBool());
  REQUIRE(c.browseActions("no-such-collection")["playNow"].toBool() == false);
}

TEST_CASE("previews only start while idle and stop when playback starts", "[ui]") {
  Fixture f;
  AppController c(&f.model);
  QSignalSpy started(&c, &AppController::previewRequested);
  QSignalSpy stopped(&c, &AppController::previewStopped);

  c.browsing("night-drive");
  REQUIRE(started.count() == 1);
  c.browsing("night-drive");  // same item again: no new request
  REQUIRE(started.count() == 1);
  c.browsing("redline");
  REQUIRE(started.count() == 2);

  c.playNow("redline");  // starting playback ends the preview
  REQUIRE(stopped.count() == 1);
  c.browsing("glass-garden");  // now something is playing: no preview
  REQUIRE(started.count() == 2);
}

TEST_CASE("leaving the swipe view stops the preview", "[ui]") {
  Fixture f;
  AppController c(&f.model);
  QSignalSpy stopped(&c, &AppController::previewStopped);
  c.stopBrowsing();  // nothing to stop
  REQUIRE(stopped.count() == 0);
  c.browsing("night-drive");
  c.stopBrowsing();
  REQUIRE(stopped.count() == 1);
}

TEST_CASE("the queue plays in order and play now interrupts", "[ui]") {
  Fixture f;
  AppController c(&f.model);
  QSignalSpy plays(&c, &AppController::playRequested);
  QSignalSpy stops(&c, &AppController::stopped);

  c.playNow("night-drive");
  REQUIRE(c.playing());
  REQUIRE(c.nowPlayingTitle() == "Night Drive");

  c.addToQueue("redline");
  c.addToQueue("glass-garden");
  c.addToQueue("interactive");  // ignored: interactive cannot be queued
  REQUIRE(c.upcomingTitles() == QStringList{"Redline", "Glass Garden"});
  REQUIRE(c.queueLength() == 2);

  c.playNow("after-hours");  // interrupts, keeps the queue
  REQUIRE(c.nowPlayingId() == "after-hours");
  REQUIRE(c.queueLength() == 2);

  c.next();
  REQUIRE(c.nowPlayingId() == "redline");
  c.next();
  REQUIRE(c.nowPlayingId() == "glass-garden");
  c.next();
  REQUIRE_FALSE(c.playing());
  REQUIRE(stops.count() == 1);
  REQUIRE(plays.count() == 4);  // night-drive, after-hours, redline, glass-garden
  REQUIRE(plays.last().at(0).toString() == "glass-garden");
}

TEST_CASE("unknown collections are ignored", "[ui]") {
  Fixture f;
  AppController c(&f.model);
  QSignalSpy plays(&c, &AppController::playRequested);
  c.playNow("nope");
  REQUIRE_FALSE(c.playing());
  REQUIRE(plays.count() == 0);
}
