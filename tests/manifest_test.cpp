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

bool has(const ParseResult& r, Severity sev, std::string_view path_part, std::string_view msg_part) {
  for (const auto& d : r.diagnostics) {
    if (d.severity == sev && d.path.find(path_part) != std::string::npos &&
        d.message.find(msg_part) != std::string::npos) {
      return true;
    }
  }
  return false;
}

const std::string kAlbumHeader = R"(
format = 1
type = "album"
title = "T"
cover = "c.jpg"
)";

std::string album(const std::string& track_body) {
  return kAlbumHeader + "[[track]]\nfile = \"a.flac\"\n" + track_body;
}

}  // namespace

TEST_CASE("example album parses with the expected values", "[manifest]") {
  const auto r = parse_manifest(read_file(kRoot / "examples/collections/night-drive/collection.toml"));
  REQUIRE(r.ok());
  REQUIRE(r.diagnostics.empty());
  const Collection& c = *r.collection;

  REQUIRE(c.type == CollectionType::Album);
  REQUIRE(c.title == "Night Drive");
  REQUIRE(c.tags == std::vector<std::string>{"pleasing", "edm"});
  REQUIRE(c.default_transition.kind == Transition::Kind::Blend);
  REQUIRE(c.default_transition.seconds == 2.0);
  REQUIRE(c.seed == parse_manifest(read_file(kRoot / "examples/collections/night-drive/collection.toml"))
                        .collection->seed);

  REQUIRE(c.preview.has_value());
  REQUIRE(c.preview->track == 1);
  REQUIRE(c.preview->start == 2'160'000);
  REQUIRE(c.preview->length == 960'000);
  REQUIRE(c.preview->preset == "presets/flow.milk");

  REQUIRE(c.tracks.size() == 2);
  const Track& t1 = c.tracks[0];
  REQUIRE(t1.file == "01-intro.flac");
  REQUIRE(t1.grid.bpm == 122.0);
  REQUIRE(t1.cues.size() == 2);
  REQUIRE(t1.cues[0].at == 0);
  REQUIRE(t1.cues[0].preset == "presets/tunnel.milk");
  REQUIRE(t1.cues[0].transition->kind == Transition::Kind::Cut);
  REQUIRE(t1.cues[1].at == 755'410);
  REQUIRE(t1.cues[1].pool->kind == PoolExpr::Kind::Tag);
  REQUIRE(t1.cues[1].pool->arg == "ambient");
  REQUIRE(t1.cues[1].every == 755'410);

  const Track& t2 = c.tracks[1];
  REQUIRE(t2.grid.downbeat_offset == 12'000);
  REQUIRE(t2.cues[0].at == 0);
  REQUIRE(t2.cues[1].at == 1'508'250);
  REQUIRE(t2.cues[1].transition->seconds == 0.5);
}

TEST_CASE("playlist fixture parses", "[manifest]") {
  const auto r = parse_manifest(read_file(kRoot / "tests/fixtures/playlist/collection.toml"));
  REQUIRE(r.ok());
  REQUIRE(r.diagnostics.empty());
  const Collection& c = *r.collection;
  REQUIRE(c.type == CollectionType::Playlist);
  REQUIRE(c.tracks[0].from == "night-drive/01-intro.flac");
  REQUIRE(c.tracks[0].file.empty());
  REQUIRE(c.preview->track == 1);
  REQUIRE(c.tracks[0].cues[0].pool->kind == PoolExpr::Kind::Dir);
}

TEST_CASE("interactive fixture parses and warns about the missing cover", "[manifest]") {
  const auto r = parse_manifest(read_file(kRoot / "tests/fixtures/interactive/collection.toml"));
  REQUIRE(r.ok());
  REQUIRE(has(r, Severity::Warning, "cover", "default interactive artwork"));
  const Collection& c = *r.collection;
  REQUIRE(c.type == CollectionType::Interactive);
  REQUIRE(c.tracks.empty());
  REQUIRE(c.preset_pool->kind == PoolExpr::Kind::Tag);
  REQUIRE(c.preset_pool->arg == "reactive");
  REQUIRE(c.preview->preset == "presets/flow.milk");
}

TEST_CASE("minimal album is valid", "[manifest]") {
  const auto r = parse_manifest(album(""));
  REQUIRE(r.ok());
  REQUIRE(r.collection->default_transition.kind == Transition::Kind::Blend);
}

TEST_CASE("invalid manifests produce diagnostics with stable paths", "[manifest]") {
  struct Case {
    const char* name;
    std::string toml;
    const char* path;
    const char* message;
  };
  const std::vector<Case> cases = {
      {"missing title", "format = 1\ntype = \"album\"\ncover = \"c.jpg\"\n[[track]]\nfile = \"a.flac\"\n",
       "title", "missing required"},
      {"wrong title type", "format = 1\ntype = \"album\"\ntitle = 5\ncover = \"c.jpg\"\n[[track]]\nfile = \"a\"\n",
       "title", "must be a string"},
      {"unsupported format", "format = 2\ntype = \"album\"\ntitle = \"T\"\n", "format", "unsupported"},
      {"bad type", "format = 1\ntype = \"single\"\ntitle = \"T\"\n", "type", "must be"},
      {"album without tracks", kAlbumHeader, "track", "at least one"},
      {"empty track array", kAlbumHeader + "track = []\n", "track", "at least one"},
      {"unsafe cover", "format = 1\ntype = \"album\"\ntitle = \"T\"\ncover = \"../x.jpg\"\n[[track]]\nfile = \"a\"\n",
       "cover", "relative path"},
      {"album track with from", kAlbumHeader + "[[track]]\nfile = \"a.flac\"\nfrom = \"x/y.flac\"\n",
       "track[1].from", "only valid in playlists"},
      {"album track without file", kAlbumHeader + "[[track]]\ntitle = \"x\"\n", "track[1].file",
       "missing required"},
      {"track file escapes", kAlbumHeader + "[[track]]\nfile = \"../a.flac\"\n", "track[1].file",
       "relative path"},
      {"playlist track with file",
       "format = 1\ntype = \"playlist\"\ntitle = \"T\"\ncover = \"c.jpg\"\n[[track]]\nfile = \"a.flac\"\n",
       "track[1].file", "only valid in albums"},
      {"playlist from without collection",
       "format = 1\ntype = \"playlist\"\ntitle = \"T\"\ncover = \"c.jpg\"\n[[track]]\nfrom = \"a.flac\"\n",
       "track[1].from", "<collection>/<file>"},
      {"interactive with tracks",
       "format = 1\ntype = \"interactive\"\ntitle = \"T\"\npreset_pool = \"all\"\n[[track]]\nfile = \"a\"\n",
       "track", "no tracks"},
      {"interactive without pool", "format = 1\ntype = \"interactive\"\ntitle = \"T\"\n", "preset_pool",
       "missing required"},
      {"interactive bad pool", "format = 1\ntype = \"interactive\"\ntitle = \"T\"\npreset_pool = \"genre:x\"\n",
       "preset_pool", "must be"},
      {"cue with both preset and pool",
       album("[[track.cue]]\nat = \"0:00\"\npreset = \"a.milk\"\npool = \"all\"\n"), "cue[1]",
       "exactly one"},
      {"cue with neither", album("[[track.cue]]\nat = \"0:00\"\n"), "cue[1]", "exactly one"},
      {"every without pool", album("[[track.cue]]\nat = \"0:00\"\npreset = \"a.milk\"\nevery = \"8 bars\"\n"),
       "cue[1].every", "only valid together"},
      {"bad every unit", album("bpm = 120\n[[track.cue]]\nat = \"0:00\"\npool = \"all\"\nevery = \"8 parsecs\"\n"),
       "cue[1].every", "unit"},
      {"cues out of order",
       album("[[track.cue]]\nat = \"0:10\"\npreset = \"a.milk\"\n[[track.cue]]\nat = \"0:05\"\npreset = \"b.milk\"\n"),
       "cue[2].at", "increasing"},
      {"duplicate cue time",
       album("[[track.cue]]\nat = \"0:10\"\npreset = \"a.milk\"\n[[track.cue]]\nat = \"0:10\"\npreset = \"b.milk\"\n"),
       "cue[2].at", "increasing"},
      {"musical cue without bpm", album("[[track.cue]]\nat = \"bar 3\"\npreset = \"a.milk\"\n"),
       "cue[1].at", "bpm"},
      {"bad transition", album("[[track.cue]]\nat = \"0:00\"\npreset = \"a.milk\"\ntransition = \"fade\"\n"),
       "cue[1].transition", "cut"},
      {"bad blend duration", album("[[track.cue]]\nat = \"0:00\"\npreset = \"a.milk\"\ntransition = \"blend:0\"\n"),
       "cue[1].transition", "seconds"},
      {"bad pool", album("[[track.cue]]\nat = \"0:00\"\npool = \"genre:x\"\n"), "cue[1].pool", "must be"},
      {"preset escapes library", album("[[track.cue]]\nat = \"0:00\"\npreset = \"../a.milk\"\n"),
       "cue[1].preset", "relative path"},
      {"bpm zero", album("bpm = 0\n"), "track[1].bpm", "(0, 400]"},
      {"bad meter", album("beats_per_bar = 1\n"), "track[1].beats_per_bar", "between 2 and 12"},
  };
  for (const auto& c : cases) {
    INFO(c.name);
    const auto r = parse_manifest(c.toml);
    REQUIRE_FALSE(r.ok());
    REQUIRE(has(r, Severity::Error, c.path, c.message));
  }
}

TEST_CASE("preview validation", "[manifest]") {
  const std::string head = "format = 1\ntype = \"album\"\ntitle = \"T\"\ncover = \"c.jpg\"\n";
  const std::string tracks = "[[track]]\nfile = \"a.flac\"\n";

  SECTION("track out of range") {
    const auto r = parse_manifest(head + "preview = { track = 3, start = \"0:00\", length = \"5s\" }\n" + tracks);
    REQUIRE_FALSE(r.ok());
    REQUIRE(has(r, Severity::Error, "preview.track", "existing track"));
  }
  SECTION("start and length are required for albums") {
    const auto r = parse_manifest(head + "preview = { track = 1 }\n" + tracks);
    REQUIRE(has(r, Severity::Error, "preview.start", "missing required"));
    REQUIRE(has(r, Severity::Error, "preview.length", "missing required"));
  }
  SECTION("musical preview times are rejected") {
    const auto r = parse_manifest(head + "preview = { start = \"bar 2\", length = \"5s\" }\n" + tracks);
    REQUIRE(has(r, Severity::Error, "preview.start", "bpm"));
  }
  SECTION("interactive preview needs a preset and no audio fields") {
    const auto r = parse_manifest(
        "format = 1\ntype = \"interactive\"\ntitle = \"T\"\npreset_pool = \"all\"\n"
        "preview = { start = \"0:00\" }\n");
    REQUIRE(has(r, Severity::Error, "preview.preset", "missing required"));
    REQUIRE(has(r, Severity::Error, "preview.start", "no audio"));
  }
}

TEST_CASE("TOML syntax errors are reported with a line number", "[manifest]") {
  const auto r = parse_manifest("format = 1\ntitle = \n");
  REQUIRE_FALSE(r.ok());
  REQUIRE(r.diagnostics.size() == 1);
  REQUIRE(r.diagnostics[0].line == 2);
  REQUIRE_FALSE(r.diagnostics[0].message.empty());
}

TEST_CASE("warnings do not block a collection", "[manifest]") {
  SECTION("unknown key") {
    const auto r = parse_manifest(kAlbumHeader + "colour = \"red\"\n[[track]]\nfile = \"a.flac\"\n");
    REQUIRE(r.ok());
    REQUIRE(has(r, Severity::Warning, "colour", "unknown key"));
  }
  SECTION("first cue after the start") {
    const auto r = parse_manifest(album("[[track.cue]]\nat = \"0:05\"\npreset = \"a.milk\"\n"));
    REQUIRE(r.ok());
    REQUIRE(has(r, Severity::Warning, "track[1]", "first cue"));
  }
  SECTION("preset without .milk extension") {
    const auto r = parse_manifest(album("[[track.cue]]\nat = \"0:00\"\npreset = \"a.txt\"\n"));
    REQUIRE(r.ok());
    REQUIRE(has(r, Severity::Warning, "cue[1].preset", ".milk"));
  }
}

TEST_CASE("pool and transition grammar", "[manifest]") {
  REQUIRE(parse_pool("all")->kind == PoolExpr::Kind::All);
  REQUIRE(parse_pool("tag:edm")->arg == "edm");
  REQUIRE(parse_pool("dir:presets/ambient")->kind == PoolExpr::Kind::Dir);
  REQUIRE_FALSE(parse_pool("tag:").has_value());
  REQUIRE_FALSE(parse_pool("").has_value());
  REQUIRE(parse_transition("cut")->kind == Transition::Kind::Cut);
  REQUIRE(parse_transition("blend:2.5")->seconds == 2.5);
  REQUIRE_FALSE(parse_transition("blend:").has_value());
  REQUIRE_FALSE(parse_transition("blend:-1").has_value());
  REQUIRE_FALSE(parse_transition("blend:61").has_value());
}

TEST_CASE("check_files reports missing cover and audio", "[manifest]") {
  namespace fs = std::filesystem;
  const fs::path base = fs::temp_directory_path() / "rm_manifest_test";
  fs::remove_all(base);
  fs::create_directories(base / "night-drive");
  fs::create_directories(base / "sunday");
  auto touch = [](const fs::path& p) { std::ofstream(p).put('x'); };

  const auto album_result =
      parse_manifest(read_file(kRoot / "examples/collections/night-drive/collection.toml"));
  const auto playlist_result = parse_manifest(read_file(kRoot / "tests/fixtures/playlist/collection.toml"));
  REQUIRE(album_result.ok());
  REQUIRE(playlist_result.ok());

  std::vector<Diagnostic> diags;
  check_files(*album_result.collection, base / "night-drive", diags);
  REQUIRE(diags.size() == 3);  // cover + two audio files

  touch(base / "night-drive/cover.svg");
  touch(base / "night-drive/01-intro.flac");
  touch(base / "night-drive/02-drive.flac");
  diags.clear();
  check_files(*album_result.collection, base / "night-drive", diags);
  REQUIRE(diags.empty());

  // Playlist tracks resolve against the library root, so remove the file the playlist refers to.
  fs::remove(base / "night-drive/01-intro.flac");
  diags.clear();
  touch(base / "sunday/cover.jpg");
  check_files(*playlist_result.collection, base / "sunday", diags);
  REQUIRE(diags.size() == 1);
  REQUIRE(diags[0].path == "track[1].from");

  touch(base / "night-drive/01-intro.flac");
  diags.clear();
  check_files(*playlist_result.collection, base / "sunday", diags);
  REQUIRE(diags.empty());

  fs::remove_all(base);
}
