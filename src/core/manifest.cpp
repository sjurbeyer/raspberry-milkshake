#include "core/manifest.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>

namespace rm {

std::string_view to_string(CollectionType type) {
  switch (type) {
    case CollectionType::Album: return "album";
    case CollectionType::Playlist: return "playlist";
    case CollectionType::Interactive: return "interactive";
  }
  return "unknown";
}

std::expected<Transition, std::string> parse_transition(std::string_view text) {
  if (text == "cut") return Transition{Transition::Kind::Cut, 0.0};
  constexpr std::string_view kBlend = "blend:";
  if (text.starts_with(kBlend)) {
    const std::string_view num = text.substr(kBlend.size());
    double seconds = 0;
    const auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), seconds);
    if (ec == std::errc{} && ptr == num.data() + num.size() && std::isfinite(seconds) &&
        seconds > 0 && seconds <= 60) {
      return Transition{Transition::Kind::Blend, seconds};
    }
    return std::unexpected("blend duration must be a number of seconds in (0, 60]");
  }
  return std::unexpected("transition must be \"cut\" or \"blend:<seconds>\"");
}

std::expected<PoolExpr, std::string> parse_pool(std::string_view text) {
  if (text == "all") return PoolExpr{PoolExpr::Kind::All, {}};
  const auto colon = text.find(':');
  if (colon != std::string_view::npos && colon + 1 < text.size()) {
    const std::string_view kind = text.substr(0, colon);
    const std::string arg(text.substr(colon + 1));
    if (kind == "tag") return PoolExpr{PoolExpr::Kind::Tag, arg};
    if (kind == "dir") return PoolExpr{PoolExpr::Kind::Dir, arg};
  }
  return std::unexpected("pool must be \"all\", \"tag:<name>\" or \"dir:<path>\"");
}

namespace {

struct Sink {
  std::vector<Diagnostic>& out;

  void error(std::string path, int line, std::string message) {
    out.push_back({Severity::Error, std::move(path), std::move(message), line});
  }
  void warn(std::string path, int line, std::string message) {
    out.push_back({Severity::Warning, std::move(path), std::move(message), line});
  }
  bool has_errors() const {
    return std::any_of(out.begin(), out.end(),
                       [](const Diagnostic& d) { return d.severity == Severity::Error; });
  }
};

int line_of(const toml::node& node) { return static_cast<int>(node.source().begin.line); }

std::string join(const std::string& base, std::string_view key) {
  return base.empty() ? std::string(key) : base + "." + std::string(key);
}

// Relative paths inside a collection must not escape it (manifests may come from USB sticks).
bool is_safe_relative(std::string_view p) {
  if (p.empty() || p.front() == '/' || p.front() == '\\' || p.find(':') != std::string_view::npos) {
    return false;
  }
  std::size_t start = 0;
  for (std::size_t i = 0; i <= p.size(); ++i) {
    if (i == p.size() || p[i] == '/' || p[i] == '\\') {
      if (p.substr(start, i - start) == "..") return false;
      start = i + 1;
    }
  }
  return true;
}

std::uint64_t fnv1a(std::string_view s) {
  std::uint64_t h = 1469598103934665603ULL;
  for (const unsigned char c : s) {
    h ^= c;
    h *= 1099511628211ULL;
  }
  return h;
}

// Typed access to one TOML table that reports problems with a stable key path.
class Reader {
 public:
  Reader(Sink& sink, const toml::table& table, std::string path)
      : sink_(sink), table_(table), path_(std::move(path)) {}

  bool has(std::string_view key) const { return table_.get(key) != nullptr; }

  std::optional<std::string> string(std::string_view key, bool required = false) {
    const toml::node* n = find(key, required);
    if (!n) return std::nullopt;
    if (const auto* s = n->as_string()) return std::string(s->get());
    sink_.error(join(path_, key), line_of(*n), "must be a string");
    return std::nullopt;
  }

  std::optional<long long> integer(std::string_view key, bool required = false) {
    const toml::node* n = find(key, required);
    if (!n) return std::nullopt;
    if (const auto* i = n->as_integer()) return static_cast<long long>(i->get());
    sink_.error(join(path_, key), line_of(*n), "must be an integer");
    return std::nullopt;
  }

  std::optional<double> number(std::string_view key) {
    const toml::node* n = find(key, false);
    if (!n) return std::nullopt;
    if (n->is_number()) return n->value<double>();
    sink_.error(join(path_, key), line_of(*n), "must be a number");
    return std::nullopt;
  }

  const toml::table* table(std::string_view key) {
    const toml::node* n = find(key, false);
    if (!n) return nullptr;
    if (const auto* t = n->as_table()) return t;
    sink_.error(join(path_, key), line_of(*n), "must be a table");
    return nullptr;
  }

  const toml::array* array(std::string_view key) {
    const toml::node* n = find(key, false);
    if (!n) return nullptr;
    if (const auto* a = n->as_array()) return a;
    sink_.error(join(path_, key), line_of(*n), "must be an array");
    return nullptr;
  }

  void forbid(std::string_view key, std::string_view why) {
    if (const toml::node* n = table_.get(key)) {
      sink_.error(join(path_, key), line_of(*n), std::string(why));
    }
  }

  void warn_unknown(std::initializer_list<std::string_view> allowed) {
    for (auto&& [key, node] : table_) {
      if (std::find(allowed.begin(), allowed.end(), key.str()) == allowed.end()) {
        sink_.warn(join(path_, key.str()), line_of(node), "unknown key (ignored)");
      }
    }
  }

  void error(std::string_view key, std::string message) {
    const toml::node* n = table_.get(key);
    sink_.error(join(path_, key), n ? line_of(*n) : line_of(table_), std::move(message));
  }

  int line(std::string_view key) const {
    const toml::node* n = table_.get(key);
    return n ? line_of(*n) : line_of(table_);
  }

  Sink& sink() { return sink_; }
  const std::string& path() const { return path_; }

 private:
  const toml::node* find(std::string_view key, bool required) {
    const toml::node* n = table_.get(key);
    if (!n && required) {
      sink_.error(join(path_, key), line_of(table_), "missing required key");
    }
    return n;
  }

  Sink& sink_;
  const toml::table& table_;
  std::string path_;
};

std::optional<Cue> parse_cue(Sink& sink, const toml::table& table, const std::string& path,
                             const MusicalContext& grid) {
  Reader r(sink, table, path);
  r.warn_unknown({"at", "preset", "pool", "every", "transition"});

  Cue cue;
  bool ok = true;

  if (const auto at = r.string("at", true)) {
    if (const auto pos = parse_cue_time(*at, grid)) {
      cue.at = *pos;
    } else {
      r.error("at", pos.error());
      ok = false;
    }
  } else {
    ok = false;
  }

  const auto preset = r.string("preset");
  const auto pool = r.string("pool");
  if (preset.has_value() == pool.has_value()) {
    sink.error(path, line_of(table), "cue needs exactly one of \"preset\" or \"pool\"");
    ok = false;
  }
  if (preset) {
    cue.preset = *preset;
    if (!is_safe_relative(*preset)) {
      r.error("preset", "must be a relative path inside the preset library");
      ok = false;
    } else if (!preset->ends_with(".milk")) {
      sink.warn(join(path, "preset"), r.line("preset"), "preset file does not end in .milk");
    }
  }
  if (pool) {
    if (const auto expr = parse_pool(*pool)) {
      cue.pool = *expr;
    } else {
      r.error("pool", expr.error());
      ok = false;
    }
  }

  if (const auto every = r.string("every")) {
    if (!pool) {
      r.error("every", "\"every\" is only valid together with \"pool\"");
      ok = false;
    } else if (const auto d = parse_duration(*every, grid)) {
      cue.every = *d;
    } else {
      r.error("every", d.error());
      ok = false;
    }
  }

  if (const auto tr = r.string("transition")) {
    if (const auto t = parse_transition(*tr)) {
      cue.transition = *t;
    } else {
      r.error("transition", t.error());
      ok = false;
    }
  }
  return ok ? std::optional<Cue>(cue) : std::nullopt;
}

Track parse_track(Sink& sink, const toml::table& table, std::size_t number, CollectionType type) {
  const std::string path = "track[" + std::to_string(number) + "]";
  Reader r(sink, table, path);
  r.warn_unknown({"file", "from", "title", "bpm", "beats_per_bar", "downbeat_offset", "cue"});

  Track track;
  if (type == CollectionType::Album) {
    r.forbid("from", "\"from\" is only valid in playlists; albums use \"file\"");
    if (const auto file = r.string("file", true)) {
      track.file = *file;
      if (!is_safe_relative(*file)) r.error("file", "must be a relative path inside the collection");
    }
  } else {
    r.forbid("file", "\"file\" is only valid in albums; playlists use \"from\"");
    if (const auto from = r.string("from", true)) {
      track.from = *from;
      if (!is_safe_relative(*from) || from->find('/') == std::string::npos) {
        r.error("from", "must look like \"<collection>/<file>\" with no \"..\" or absolute parts");
      }
    }
  }
  track.title = r.string("title").value_or("");

  if (const auto bpm = r.number("bpm")) {
    if (*bpm > 0 && *bpm <= 400) {
      track.grid.bpm = *bpm;
    } else {
      r.error("bpm", "must be in (0, 400]");
    }
  }
  if (const auto bpb = r.integer("beats_per_bar")) {
    if (*bpb >= 2 && *bpb <= 12) {
      track.grid.beats_per_bar = static_cast<int>(*bpb);
    } else {
      r.error("beats_per_bar", "must be between 2 and 12");
    }
  }
  if (const auto off = r.string("downbeat_offset")) {
    if (const auto pos = parse_cue_time(*off, MusicalContext{})) {
      track.grid.downbeat_offset = *pos;
    } else {
      r.error("downbeat_offset", pos.error());
    }
  }

  if (const toml::array* cues = r.array("cue")) {
    if (!cues->is_array_of_tables()) {
      r.error("cue", "must be an array of tables ([[track.cue]])");
    } else {
      std::optional<SamplePos> previous;
      for (std::size_t i = 0; i < cues->size(); ++i) {
        const toml::table& ct = *cues->get(i)->as_table();
        const std::string cue_path = path + ".cue[" + std::to_string(i + 1) + "]";
        const auto cue = parse_cue(sink, ct, cue_path, track.grid);
        if (!cue) continue;
        if (previous && cue->at <= *previous) {
          sink.error(join(cue_path, "at"), line_of(ct),
                     "cues must be in strictly increasing time order");
          continue;
        }
        previous = cue->at;
        track.cues.push_back(*cue);
      }
      if (!track.cues.empty() && track.cues.front().at > 0) {
        sink.warn(path, line_of(table), "first cue is after the start; no preset is set before it");
      }
    }
  }
  return track;
}

void parse_preview(Sink& sink, const toml::table& table, CollectionType type,
                   std::size_t track_count, Collection& out) {
  Reader r(sink, table, "preview");
  Preview preview;
  bool ok = true;

  if (type == CollectionType::Interactive) {
    r.warn_unknown({"preset"});
    r.forbid("track", "interactive collections have no tracks to preview");
    r.forbid("start", "interactive collections have no audio to preview");
    r.forbid("length", "interactive collections have no audio to preview");
    if (const auto p = r.string("preset", true)) preview.preset = *p; else ok = false;
  } else {
    r.warn_unknown({"track", "start", "length", "preset"});
    if (const auto t = r.integer("track")) {
      if (*t >= 1 && static_cast<std::size_t>(*t) <= track_count) {
        preview.track = static_cast<int>(*t);
      } else {
        r.error("track", "must refer to an existing track (1-based)");
        ok = false;
      }
    }
    if (const auto s = r.string("start", true)) {
      if (const auto pos = parse_cue_time(*s, MusicalContext{})) {
        preview.start = *pos;
      } else {
        r.error("start", pos.error());
        ok = false;
      }
    } else {
      ok = false;
    }
    if (const auto l = r.string("length", true)) {
      if (const auto d = parse_duration(*l, MusicalContext{})) {
        preview.length = *d;
      } else {
        r.error("length", d.error());
        ok = false;
      }
    } else {
      ok = false;
    }
    preview.preset = r.string("preset").value_or("");
  }
  if (!preview.preset.empty() && !is_safe_relative(preview.preset)) {
    r.error("preset", "must be a relative path inside the preset library");
    ok = false;
  }
  if (ok) out.preview = preview;
}

}  // namespace

ParseResult parse_manifest(std::string_view text) {
  ParseResult result;
  Sink sink{result.diagnostics};

  toml::table root;
  try {
    root = toml::parse(text);
  } catch (const toml::parse_error& e) {
    sink.error("", static_cast<int>(e.source().begin.line), std::string(e.description()));
    return result;
  }

  Reader top(sink, root, "");
  top.warn_unknown({"format", "type", "title", "artist", "tags", "cover", "preview", "defaults",
                    "seed", "preset_pool", "track"});

  if (const auto format = top.integer("format", true); format && *format != 1) {
    top.error("format", "unsupported format version (this build reads format = 1)");
  }

  Collection c;
  const auto type_text = top.string("type", true);
  if (type_text == "album") c.type = CollectionType::Album;
  else if (type_text == "playlist") c.type = CollectionType::Playlist;
  else if (type_text == "interactive") c.type = CollectionType::Interactive;
  else {
    if (type_text) top.error("type", "must be \"album\", \"playlist\" or \"interactive\"");
    return result;  // nothing else can be validated without knowing the type
  }

  if (const auto title = top.string("title", true)) {
    if (title->empty()) top.error("title", "must not be empty");
    c.title = *title;
  }
  c.artist = top.string("artist").value_or("");
  c.seed = fnv1a(c.title);
  if (const auto seed = top.integer("seed")) c.seed = static_cast<std::uint64_t>(*seed);

  if (const toml::array* tags = top.array("tags")) {
    for (std::size_t i = 0; i < tags->size(); ++i) {
      const auto* s = tags->get(i)->as_string();
      if (s && !s->get().empty()) {
        c.tags.push_back(s->get());
      } else {
        sink.error("tags[" + std::to_string(i + 1) + "]", line_of(*tags->get(i)),
                   "tags must be non-empty strings");
      }
    }
  }

  if (const auto cover = top.string("cover", c.type != CollectionType::Interactive)) {
    c.cover = *cover;
    if (!is_safe_relative(*cover)) top.error("cover", "must be a relative path inside the collection");
  } else if (c.type == CollectionType::Interactive && !top.has("cover")) {
    sink.warn("cover", 0, "no cover set; the default interactive artwork will be used");
  }

  if (const toml::table* defaults = top.table("defaults")) {
    Reader dr(sink, *defaults, "defaults");
    dr.warn_unknown({"transition"});
    if (const auto t = dr.string("transition")) {
      if (const auto tr = parse_transition(*t)) c.default_transition = *tr;
      else dr.error("transition", tr.error());
    }
  }

  if (c.type == CollectionType::Interactive) {
    top.forbid("track", "interactive collections have no tracks");
    if (const auto pool = top.string("preset_pool", true)) {
      if (const auto expr = parse_pool(*pool)) c.preset_pool = *expr;
      else top.error("preset_pool", expr.error());
    }
  } else {
    if (top.has("preset_pool")) {
      sink.warn("preset_pool", top.line("preset_pool"), "only used by interactive collections (ignored)");
    }
    const toml::array* tracks = top.array("track");
    if (!tracks) {
      if (!top.has("track")) sink.error("track", line_of(root), "at least one [[track]] is required");
    } else if (tracks->empty()) {
      top.error("track", "at least one [[track]] is required");
    } else if (!tracks->is_array_of_tables()) {
      top.error("track", "must be an array of tables ([[track]])");
    } else {
      for (std::size_t i = 0; i < tracks->size(); ++i) {
        c.tracks.push_back(parse_track(sink, *tracks->get(i)->as_table(), i + 1, c.type));
      }
    }
  }

  if (const toml::table* preview = top.table("preview")) {
    parse_preview(sink, *preview, c.type, c.tracks.size(), c);
  }

  if (!sink.has_errors()) result.collection = std::move(c);
  return result;
}

void check_files(const Collection& collection, const std::filesystem::path& collection_dir,
                 std::vector<Diagnostic>& out) {
  namespace fs = std::filesystem;
  auto require = [&](const fs::path& file, std::string path, const char* what) {
    if (!fs::is_regular_file(file)) {
      out.push_back({Severity::Error, std::move(path),
                     std::string(what) + " not found: " + file.generic_string(), 0});
    }
  };
  if (!collection.cover.empty()) require(collection_dir / collection.cover, "cover", "cover image");

  const fs::path library_root = collection_dir.parent_path();
  for (std::size_t i = 0; i < collection.tracks.size(); ++i) {
    const Track& t = collection.tracks[i];
    const std::string path = "track[" + std::to_string(i + 1) + "]";
    if (collection.type == CollectionType::Album) {
      require(collection_dir / t.file, path + ".file", "audio file");
    } else {
      require(library_root / t.from, path + ".from", "audio file");
    }
  }
}

}  // namespace rm
