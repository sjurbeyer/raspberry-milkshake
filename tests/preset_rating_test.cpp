#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/preset_rating.hpp"

using namespace rm;
namespace fs = std::filesystem;

namespace {

struct TempDir {
  fs::path path = fs::temp_directory_path() / "rm_preset_rating_test";
  TempDir() {
    fs::remove_all(path);
    fs::create_directories(path);
  }
  ~TempDir() { fs::remove_all(path); }
  fs::path write(const std::string& name, const std::string& content) const {
    const fs::path p = path / name;
    std::ofstream(p, std::ios::binary) << content;
    return p;
  }
};

std::string slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

TEST_CASE("ratings are read from the fRating line", "[rating]") {
  REQUIRE(parse_rating("[preset00]\nfRating=5.000000\nfDecay=0.9\n") == 5.0);
  REQUIRE(parse_rating("fRating=0.000000") == 0.0);
  REQUIRE(parse_rating("[preset00]\r\nfRating=2.500000\r\n") == 2.5);
  REQUIRE(parse_rating("  fRating = 3  \n") == 3.0);
}

TEST_CASE("a missing or unreadable rating is nullopt", "[rating]") {
  REQUIRE_FALSE(parse_rating("[preset00]\nfDecay=0.9\n").has_value());
  REQUIRE_FALSE(parse_rating("fRating=abc\n").has_value());
  REQUIRE_FALSE(parse_rating("fRatingX=4\n").has_value());  // a different key
  REQUIRE_FALSE(parse_rating("").has_value());
  REQUIRE_FALSE(read_rating("/no/such/file.milk").has_value());
}

TEST_CASE("writing a rating changes only that line", "[rating]") {
  const TempDir dir;
  const std::string original = "MILKDROP_PRESET_VERSION=201\n[preset00]\nfRating=5.000000\nfDecay=0.980000\nper_frame_1=zoom=1;\n";
  const fs::path file = dir.write("a.milk", original);

  std::string error;
  REQUIRE(write_rating(file, 0, &error));
  REQUIRE(error.empty());
  REQUIRE(slurp(file) == "MILKDROP_PRESET_VERSION=201\n[preset00]\nfRating=0.000000\nfDecay=0.980000\nper_frame_1=zoom=1;\n");
  REQUIRE(read_rating(file) == 0.0);

  REQUIRE(write_rating(file, 5));
  REQUIRE(slurp(file) == original);  // round trip is byte identical
  REQUIRE_FALSE(fs::exists(file.string() + ".rm-tmp"));
}

TEST_CASE("windows line endings and odd bytes survive", "[rating]") {
  const TempDir dir;
  const std::string original = "[preset00]\r\nfRating=4.000000\r\nname=caf\xE9\r\n";  // a Latin-1 byte
  const fs::path file = dir.write("crlf.milk", original);
  REQUIRE(write_rating(file, 1));
  REQUIRE(slurp(file) == "[preset00]\r\nfRating=1.000000\r\nname=caf\xE9\r\n");
}

TEST_CASE("a preset without a rating gets one under [preset00]", "[rating]") {
  const TempDir dir;
  const fs::path lf = dir.write("lf.milk", "VERSION=1\n[preset00]\nfDecay=0.5\n");
  REQUIRE(write_rating(lf, 0));
  REQUIRE(slurp(lf) == "VERSION=1\n[preset00]\nfRating=0.000000\nfDecay=0.5\n");

  const fs::path crlf = dir.write("crlf.milk", "[preset00]\r\nfDecay=0.5\r\n");
  REQUIRE(write_rating(crlf, 2));
  REQUIRE(slurp(crlf) == "[preset00]\r\nfRating=2.000000\r\nfDecay=0.5\r\n");
}

TEST_CASE("bad requests are refused and leave the file alone", "[rating]") {
  const TempDir dir;
  const std::string original = "[preset00]\nfRating=3.000000\n";
  const fs::path file = dir.write("a.milk", original);
  std::string error;

  REQUIRE_FALSE(write_rating(file, 6, &error));
  REQUIRE(error.find("between 0 and 5") != std::string::npos);
  REQUIRE_FALSE(write_rating(file, -1, &error));

  const fs::path not_a_preset = dir.write("readme.txt", "hello\n");
  REQUIRE_FALSE(write_rating(not_a_preset, 0, &error));
  REQUIRE(error.find("[preset00]") != std::string::npos);
  REQUIRE(slurp(not_a_preset) == "hello\n");

  REQUIRE_FALSE(write_rating(dir.path / "missing.milk", 0, &error));
  REQUIRE(slurp(file) == original);
}
