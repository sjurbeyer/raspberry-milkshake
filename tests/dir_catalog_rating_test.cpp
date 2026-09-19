#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/dir_catalog.hpp"

using namespace rm;

namespace {

std::string preset_text(const char* rating) {
  return std::string("[preset00]\nfRating=") + rating + "\nfDecay=0.5\n";
}

// a/keep.milk rated 5, a/hide.milk rated 0, b/meh.milk rated 1, c/plain.milk without a rating.
struct RatedTree {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "rm_dir_catalog_rating_test";
  RatedTree() {
    std::filesystem::remove_all(root);
    auto put = [&](const char* rel, const std::string& text) {
      std::filesystem::create_directories((root / rel).parent_path());
      std::ofstream(root / rel, std::ios::binary) << text;
    };
    put("a/keep.milk", preset_text("5.000000"));
    put("a/hide.milk", preset_text("0.000000"));
    put("b/meh.milk", preset_text("1.000000"));
    put("c/plain.milk", "[preset00]\nfDecay=0.5\n");
  }
  ~RatedTree() { std::filesystem::remove_all(root); }
};

}  // namespace

TEST_CASE("presets rated 0 are never returned by a pool", "[catalog][rating]") {
  const RatedTree tree;
  const DirCatalog catalog(tree.root);
  REQUIRE(catalog.size() == 4);  // the hidden one is still known
  REQUIRE(catalog.hidden_count() == 1);
  REQUIRE(catalog.resolve({PoolExpr::Kind::All, {}}) ==
          std::vector<std::string>{"a/keep.milk", "b/meh.milk", "c/plain.milk"});
  REQUIRE(catalog.resolve({PoolExpr::Kind::Dir, "a"}) == std::vector<std::string>{"a/keep.milk"});
}

TEST_CASE("ratings are read from the files, with 3 when a file has none", "[catalog][rating]") {
  const RatedTree tree;
  const DirCatalog catalog(tree.root);
  REQUIRE(catalog.rating("a/keep.milk") == 5.0);
  REQUIRE(catalog.rating("a/hide.milk") == 0.0);
  REQUIRE(catalog.rating("b/meh.milk") == 1.0);
  REQUIRE(catalog.rating("c/plain.milk") == DirCatalog::kDefaultRating);
  REQUIRE_FALSE(catalog.rating("nope.milk").has_value());
}

TEST_CASE("set_rating edits the file and takes effect at once", "[catalog][rating]") {
  const RatedTree tree;
  DirCatalog catalog(tree.root);

  REQUIRE(catalog.set_rating("a/keep.milk", 0));
  REQUIRE(catalog.rating("a/keep.milk") == 0.0);
  REQUIRE(catalog.hidden_count() == 2);
  REQUIRE(catalog.resolve({PoolExpr::Kind::All, {}}) == std::vector<std::string>{"b/meh.milk", "c/plain.milk"});

  // The file itself changed, so a fresh scan (or MilkDrop) sees it too.
  REQUIRE(DirCatalog(tree.root).rating("a/keep.milk") == 0.0);

  REQUIRE(catalog.set_rating("a/hide.milk", 4));  // and back again
  REQUIRE(catalog.resolve({PoolExpr::Kind::Dir, "a"}) == std::vector<std::string>{"a/hide.milk"});
}

TEST_CASE("set_rating refuses unknown names and bad values", "[catalog][rating]") {
  const RatedTree tree;
  DirCatalog catalog(tree.root);
  std::string error;
  REQUIRE_FALSE(catalog.set_rating("../outside.milk", 0, &error));
  REQUIRE(error == "unknown preset");
  REQUIRE_FALSE(catalog.set_rating("/etc/passwd", 0, &error));
  REQUIRE_FALSE(catalog.set_rating("a/keep.milk", 9, &error));
  REQUIRE(catalog.rating("a/keep.milk") == 5.0);
}

TEST_CASE("copies of a catalog see the same ratings", "[catalog][rating]") {
  const RatedTree tree;
  DirCatalog catalog(tree.root);
  const DirCatalog copy = catalog;
  REQUIRE(catalog.set_rating("b/meh.milk", 0));
  REQUIRE(copy.rating("b/meh.milk") == 0.0);
}
