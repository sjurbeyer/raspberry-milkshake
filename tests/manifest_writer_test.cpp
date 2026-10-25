#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/manifest.hpp"

using namespace rm;

namespace {

std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

const std::filesystem::path kRoot = RM_SOURCE_DIR;

Collection parse_ok(const std::string& text) {
  auto r = parse_manifest(text);
  INFO(text);
  for (const auto& d : r.diagnostics) {
    UNSCOPED_INFO((d.severity == Severity::Error ? "error " : "warning ") << d.path << ": " << d.message);
  }
  REQUIRE(r.ok());
  return *r.collection;
}

// write -> parse gives the same collection, and writing that again gives the same text.
void require_round_trip(const Collection& c) {
  const std::string text = write_manifest(c);
  INFO(text);
  const Collection back = parse_ok(text);
  REQUIRE(back == c);
  REQUIRE(write_manifest(back) == text);
}

bool contains(const std::string& haystack, std::string_view needle) {
  return haystack.find(needle) != std::string::npos;
}

Collection album_with_cues(double bpm, int beats_per_bar, SamplePos downbeat_offset) {
  Collection c;
  c.title = "Grid";
  c.cover = "cover.jpg";
  Track t;
  t.file = "a.flac";
  t.grid = {bpm, beats_per_bar, downbeat_offset};
  c.tracks.push_back(t);
  return c;
}

}  // namespace

TEST_CASE("every example and fixture round-trips through write_manifest", "[manifest]") {
  std::vector<std::filesystem::path> dirs;
  for (const auto& e : std::filesystem::directory_iterator(kRoot / "examples/collections")) dirs.push_back(e.path());
  dirs.push_back(kRoot / "tests/fixtures/playlist");
  dirs.push_back(kRoot / "tests/fixtures/interactive");
  for (const auto& dir : dirs) {
    INFO(dir.generic_string());
    require_round_trip(parse_ok(read_file(dir / "collection.toml")));
  }
}

TEST_CASE("the example album is written in its own spelling", "[manifest]") {
  const auto c = parse_ok(read_file(kRoot / "examples/collections/night-drive/collection.toml"));
  const std::string text = write_manifest(c);
  INFO(text);
  CHECK(text.starts_with("format  = 1\ntype    = \"album\"\ntitle   = \"Night Drive\"\n"));
  CHECK(contains(text, "preview = { track = 1, start = \"0:45\", length = \"20s\", preset = \"presets/flow.milk\" }"));
  CHECK(contains(text, "\n[defaults]\ntransition = \"blend:2.0\"\n"));
  CHECK(contains(text, "  at = \"bar 9\"\n  pool = \"tag:ambient\"\n  every = \"8 bars\"\n"));
  CHECK(contains(text, "downbeat_offset = \"0:00.250\"\n"));
  CHECK(contains(text, "  at = \"bar 17 beat 3.5\"\n"));
  CHECK(contains(text, "  transition = \"blend:0.5\"\n"));
  CHECK(contains(text, "  at = \"0:00\"\n"));
  CHECK_FALSE(contains(text, "seed"));  // the default seed is implied by the title
}

TEST_CASE("times round-trip to the exact sample", "[manifest]") {
  // Positions that are not on a millisecond, a beat, or anything else tidy.
  Collection c = album_with_cues(123.456, 7, 12'345);
  Track& t = c.tracks[0];
  const SamplePos positions[] = {0, 1, 47, 48, 47'999, 48'001, 59 * kSampleRate + 47'999,
                                 3600 * kSampleRate - 1, 3600 * kSampleRate + 7, 10 * 3600 * kSampleRate + 3};
  for (const SamplePos at : positions) {
    Cue cue;
    cue.at = at;
    cue.pool = PoolExpr{PoolExpr::Kind::All, {}};
    cue.every = at + 1;  // durations must be positive
    t.cues.push_back(cue);
  }
  c.preview = Preview{1, 3 * kSampleRate + 5, 1, ""};
  require_round_trip(c);
}

TEST_CASE("cue times prefer musical spellings that are exact on the grid", "[manifest]") {
  Collection c = album_with_cues(120, 3, kSampleRate / 2);  // a beat is 24000 samples
  Track& t = c.tracks[0];
  auto add = [&](SamplePos at, std::optional<SamplePos> every = {}) {
    Cue cue;
    cue.at = at;
    if (every) {
      cue.pool = PoolExpr{PoolExpr::Kind::Dir, "presets/ambient"};
      cue.every = every;
    } else {
      cue.preset = "presets/a.milk";
    }
    t.cues.push_back(cue);
  };
  add(0);                                     // before the downbeat: clock time
  add(kSampleRate / 2 + 3 * 24'000, 3 * 24'000);       // bar 2, every 1 bar
  add(kSampleRate / 2 + 7 * 24'000 + 6'000, 24'000);   // bar 3 beat 2.25, every 1 beat
  add(kSampleRate / 2 + 9 * 24'000 + 1, 24'001);       // off the grid by one sample
  add(kSampleRate / 2 + 12 * 24'000, 6 * 24'000);      // bar 5, every 2 bars
  const std::string text = write_manifest(c);
  INFO(text);
  CHECK(contains(text, "beats_per_bar   = 3\n"));
  CHECK(contains(text, "downbeat_offset = \"0:00.500\"\n"));
  CHECK(contains(text, "at = \"0:00\"\n"));
  CHECK(contains(text, "at = \"bar 2\"\n"));
  CHECK(contains(text, "every = \"1 bar\"\n"));
  CHECK(contains(text, "at = \"bar 3 beat 2.25\"\n"));
  CHECK(contains(text, "every = \"1 beat\"\n"));
  CHECK(contains(text, "every = \"2 bars\"\n"));
  CHECK_FALSE(contains(text, "bar 4"));  // the off-grid cue falls back to an absolute time
  require_round_trip(c);
}

TEST_CASE("strings are escaped and survive a round trip", "[manifest]") {
  Collection c = album_with_cues(0, 4, 0);
  c.title = "Quote \" backslash \\ tab \t newline \n bell \x07 caf\xC3\xA9 \xE2\x99\xAB";
  c.artist = "C:\\not\\a\\path";
  c.tags = {"edm", "with space", "\"quoted\""};
  c.tracks[0].title = "Track \"one\"";
  Cue cue;
  cue.preset = "presets/sub dir/it's here.milk";
  c.tracks[0].cues.push_back(cue);
  c.seed = default_seed(c.title);
  require_round_trip(c);
}

TEST_CASE("non-default values are written and defaults are omitted", "[manifest]") {
  Collection c = album_with_cues(0, 4, 0);
  Cue cue;
  cue.preset = "presets/a.milk";
  c.tracks[0].cues.push_back(cue);
  c.seed = default_seed(c.title);

  std::string text = write_manifest(c);
  CHECK_FALSE(contains(text, "seed"));
  CHECK_FALSE(contains(text, "bpm"));
  CHECK_FALSE(contains(text, "beats_per_bar"));
  CHECK_FALSE(contains(text, "downbeat_offset"));
  CHECK_FALSE(contains(text, "artist"));
  CHECK_FALSE(contains(text, "preview"));

  for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{42}, ~std::uint64_t{0}}) {
    c.seed = seed;
    text = write_manifest(c);
    INFO(text);
    CHECK(contains(text, "seed"));
    require_round_trip(c);
  }

  c.default_transition = Transition{Transition::Kind::Cut, 0.0};
  c.tracks[0].cues[0].transition = Transition{Transition::Kind::Blend, 0.125};
  c.tracks[0].grid.bpm = 97.5;
  text = write_manifest(c);
  CHECK(contains(text, "[defaults]\ntransition = \"cut\"\n"));
  CHECK(contains(text, "transition = \"blend:0.125\"\n"));
  CHECK(contains(text, "bpm  = 97.5\n"));
  require_round_trip(c);
}

TEST_CASE("playlists and interactive collections are written with their own keys", "[manifest]") {
  Collection playlist;
  playlist.type = CollectionType::Playlist;
  playlist.title = "Mix";
  playlist.cover = "cover.jpg";
  Track t;
  t.from = "night-drive/01-intro.flac";
  Cue cue;
  cue.pool = PoolExpr{PoolExpr::Kind::All, {}};
  t.cues.push_back(cue);
  playlist.tracks.push_back(t);
  playlist.seed = default_seed(playlist.title);
  const std::string ptext = write_manifest(playlist);
  CHECK(contains(ptext, "from = \"night-drive/01-intro.flac\""));
  CHECK_FALSE(contains(ptext, "file"));
  require_round_trip(playlist);

  Collection inter;
  inter.type = CollectionType::Interactive;
  inter.title = "Interactive";
  inter.preset_pool = PoolExpr{PoolExpr::Kind::Tag, "reactive"};
  inter.preview = Preview{1, 0, 0, "presets/flow.milk"};
  inter.seed = default_seed(inter.title);
  const std::string itext = write_manifest(inter);
  INFO(itext);
  CHECK(contains(itext, "preset_pool = \"tag:reactive\"\n"));
  CHECK(contains(itext, "preview     = { preset = \"presets/flow.milk\" }\n"));
  CHECK_FALSE(contains(itext, "[[track]]"));
  // No cover is a warning, not an error, for interactive collections.
  REQUIRE(parse_manifest(itext).ok());
  require_round_trip(inter);
}
