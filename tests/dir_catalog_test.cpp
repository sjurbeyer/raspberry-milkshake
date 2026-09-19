#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/dir_catalog.hpp"

using namespace rm;

namespace {

struct TempTree {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "rm_dir_catalog_test";
  TempTree() {
    std::filesystem::remove_all(root);
    for (const char* p : {"a.milk", "ambient/b.milk", "ambient/deep/c.milk", "ambient/notes.txt",
                          "edm/d.milk", "ambientish/e.milk"}) {
      std::filesystem::create_directories((root / p).parent_path());
      std::ofstream(root / p).put('x');
    }
  }
  ~TempTree() { std::filesystem::remove_all(root); }
};

}  // namespace

TEST_CASE("the catalog lists .milk files recursively, sorted, with relative paths", "[catalog]") {
  const TempTree tree;
  const DirCatalog catalog(tree.root);
  REQUIRE(catalog.size() == 5);
  const auto all = catalog.resolve({PoolExpr::Kind::All, {}});
  REQUIRE(all == std::vector<std::string>{"a.milk", "ambient/b.milk", "ambient/deep/c.milk",
                                          "ambientish/e.milk", "edm/d.milk"});
}

TEST_CASE("dir pools match a whole directory, not a name prefix", "[catalog]") {
  const TempTree tree;
  const DirCatalog catalog(tree.root);
  const auto ambient = catalog.resolve({PoolExpr::Kind::Dir, "ambient"});
  REQUIRE(ambient == std::vector<std::string>{"ambient/b.milk", "ambient/deep/c.milk"});
  REQUIRE(catalog.resolve({PoolExpr::Kind::Dir, "ambient/"}) == ambient);
  REQUIRE(catalog.resolve({PoolExpr::Kind::Dir, "missing"}).empty());
}

TEST_CASE("tag pools are empty until rm-bench provides tags", "[catalog]") {
  const TempTree tree;
  REQUIRE(DirCatalog(tree.root).resolve({PoolExpr::Kind::Tag, "edm"}).empty());
}

TEST_CASE("a missing directory gives an empty catalog", "[catalog]") {
  const DirCatalog catalog("/definitely/not/here");
  REQUIRE(catalog.size() == 0);
}
