#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/clock.hpp"
#include "core/cue_time.hpp"

namespace rm {

enum class CollectionType { Album, Playlist, Interactive };
std::string_view to_string(CollectionType type);

enum class Severity { Error, Warning };

struct Diagnostic {
  Severity severity = Severity::Error;
  std::string path;     // e.g. "track[2].cue[1].at"; empty for whole-file problems
  std::string message;
  int line = 0;         // 1-based line in collection.toml, 0 if unknown
};

struct Transition {
  enum class Kind { Cut, Blend };
  Kind kind = Kind::Cut;
  double seconds = 0.0;  // blend duration
};

// Which presets a pool cue may pick from: "all", "tag:<name>" or "dir:<relative dir>".
struct PoolExpr {
  enum class Kind { All, Tag, Dir };
  Kind kind = Kind::All;
  std::string arg;
};

std::expected<Transition, std::string> parse_transition(std::string_view text);  // "cut" | "blend:<s>"
std::expected<PoolExpr, std::string> parse_pool(std::string_view text);

struct Cue {
  SamplePos at = 0;
  std::string preset;                 // empty when `pool` is set
  std::optional<PoolExpr> pool;
  std::optional<SamplePos> every;     // repeat interval (pool cues only)
  std::optional<Transition> transition;
};

struct Track {
  std::string file;   // album: audio file relative to the collection dir
  std::string from;   // playlist: "<collection>/<file>" relative to the library root
  std::string title;
  MusicalContext grid;
  std::vector<Cue> cues;
};

struct Preview {
  int track = 1;  // 1-based
  SamplePos start = 0;
  SamplePos length = 0;
  std::string preset;
};

struct Collection {
  CollectionType type = CollectionType::Album;
  std::string title;
  std::string artist;
  std::vector<std::string> tags;
  std::string cover;
  std::optional<Preview> preview;
  Transition default_transition{Transition::Kind::Blend, 2.0};
  std::uint64_t seed = 0;
  std::optional<PoolExpr> preset_pool;  // interactive only
  std::vector<Track> tracks;
};

struct ParseResult {
  std::optional<Collection> collection;  // present only when there are no errors
  std::vector<Diagnostic> diagnostics;
  bool ok() const { return collection.has_value(); }
};

// Never throws. Warnings do not prevent `collection` from being set.
ParseResult parse_manifest(std::string_view toml_text);

// Checks that referenced files exist. `collection_dir` is the directory holding collection.toml;
// playlist `from` paths resolve against its parent (the library root).
void check_files(const Collection& collection, const std::filesystem::path& collection_dir,
                 std::vector<Diagnostic>& out);

}  // namespace rm
