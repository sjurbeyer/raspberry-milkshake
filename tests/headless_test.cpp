#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include "app/headless.hpp"
#include "ui/library_model.hpp"

using namespace rm;

namespace {

QCoreApplication& app() {
  static int argc = 1;
  static char arg0[] = "headless_test";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication instance(argc, argv);
  return instance;
}

}  // namespace

TEST_CASE("headless is decided by screen count unless overridden", "[app][headless]") {
  REQUIRE(should_run_headless(0, false, false));
  REQUIRE_FALSE(should_run_headless(1, false, false));
  REQUIRE_FALSE(should_run_headless(2, false, false));

  REQUIRE(should_run_headless(1, true, false));    // --headless wins even with a screen present
  REQUIRE_FALSE(should_run_headless(0, false, true));  // --ui wins even with no screen detected
  REQUIRE(should_run_headless(0, true, true));     // both set: headless takes precedence
}

TEST_CASE("the default interactive collection is found by scanning the library", "[app][headless]") {
  app();
  LibraryModel model;
  model.load(QString(RM_SOURCE_DIR) + "/examples/collections");
  REQUIRE(find_default_interactive_id(model) == "interactive");
}

TEST_CASE("a library with no interactive collection yields an empty id", "[app][headless]") {
  app();
  const QString root = QDir::temp().filePath("rm_headless_test_no_interactive");
  QDir(root).removeRecursively();
  QDir().mkpath(root + "/album");
  QFile file(root + "/album/collection.toml");
  REQUIRE(file.open(QIODevice::WriteOnly));
  file.write("format = 1\ntype = \"album\"\ntitle = \"T\"\ncover = \"c.jpg\"\n[[track]]\nfile = \"a.flac\"\n");
  file.close();

  LibraryModel model;
  model.load(root);
  REQUIRE(find_default_interactive_id(model).isEmpty());
  QDir(root).removeRecursively();
}
