#include "core/manifest.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <utility>

namespace rm {
namespace {

// ---- values ----------------------------------------------------------------------------------

// A TOML basic string. Non-ASCII UTF-8 passes through unchanged.
std::string quote(std::string_view s) {
  std::string out = "\"";
  for (const char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\t': out += "\\t"; break;
      case '\n': out += "\\n"; break;
      case '\f': out += "\\f"; break;
      case '\r': out += "\\r"; break;
      default:
        if (c < 0x20 || c == 0x7f) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04X", c);
          out += buf;
        } else {
          out += ch;
        }
    }
  }
  return out + "\"";
}

// Shortest decimal that reads back as the same double ("122", "0.5", "122.75").
std::string number(double v) {
  char buf[32];
  const auto [ptr, ec] = std::to_chars(buf, buf + sizeof buf, v);
  return ec == std::errc{} ? std::string(buf, ptr) : std::string("0");
}

std::string transition_text(const Transition& t) {
  if (t.kind == Transition::Kind::Cut) return "cut";
  std::string seconds = number(t.seconds);
  if (seconds.find_first_of(".e") == std::string::npos) seconds += ".0";  // "blend:2.0", as documented
  return "blend:" + seconds;
}

std::string pool_text(const PoolExpr& p) {
  switch (p.kind) {
    case PoolExpr::Kind::All: return "all";
    case PoolExpr::Kind::Tag: return "tag:" + p.arg;
    case PoolExpr::Kind::Dir: return "dir:" + p.arg;
  }
  return "all";
}

// ---- times -----------------------------------------------------------------------------------
// Each formatter tries the most readable spelling first and keeps the first one that the parser
// maps back to exactly the same sample, so a write/parse round trip never moves a cue.

constexpr long long kPow10[] = {1, 10, 100, 1000, 10000, 100000, 1000000};

// `units / 10^digits` as a decimal with trailing zeros trimmed ("3", "3.5", "0.25").
std::string fixed(long long units, int digits) {
  std::string whole = std::to_string(units / kPow10[digits]);
  if (digits == 0) return whole;
  std::string frac = std::to_string(units % kPow10[digits]);
  frac.insert(0, static_cast<std::size_t>(digits) - frac.size(), '0');
  while (!frac.empty() && frac.back() == '0') frac.pop_back();
  return frac.empty() ? whole : whole + "." + frac;
}

std::string two_digits(long long v) { return (v < 10 ? "0" : "") + std::to_string(v); }

// "M:SS", "M:SS.fff" (more digits only when a sample needs them) or "H:MM:SS.fff"; parsed with
// no musical context.
std::string clock_time(SamplePos pos) {
  const long long whole = pos / kSampleRate;
  const long long rem = pos % kSampleRate;
  for (int d = 0; d <= 6; ++d) {
    const long long frac = std::llround(static_cast<double>(rem) * kPow10[d] / kSampleRate);
    if (frac >= kPow10[d]) continue;  // would round up into the next second
    std::string seconds = two_digits(whole % 60);
    if (d > 0) {
      const std::string f = fixed(frac, d);
      if (const auto dot = f.find('.'); dot != std::string::npos) {
        std::string frac_text = f.substr(dot);  // ".25"
        frac_text.resize(std::max<std::size_t>(frac_text.size(), 4), '0');  // at least M:SS.fff
        seconds += frac_text;
      }
    }
    const long long minutes = whole / 60;
    const std::string text = minutes < 60
        ? std::to_string(minutes) + ":" + seconds
        : std::to_string(minutes / 60) + ":" + two_digits(minutes % 60) + ":" + seconds;
    if (parse_cue_time(text, MusicalContext{}) == pos) return text;
  }
  // Unreachable for non-negative positions (6 decimals resolve every 48 kHz sample).
  return std::to_string(whole) + "s";
}

// "12.5s"; parsed as a duration (so it must stay positive).
std::string seconds_duration(SamplePos len) {
  for (int d = 0; d <= 6; ++d) {
    const long long units = std::llround(static_cast<double>(len) * kPow10[d] / kSampleRate);
    if (units <= 0) continue;
    const std::string text = fixed(units, d) + "s";
    if (parse_duration(text, MusicalContext{}) == len) return text;
  }
  return fixed(std::llround(static_cast<double>(len) * kPow10[6] / kSampleRate), 6) + "s";
}

// "bar N" or "bar N beat B", if `pos` lies exactly on such a position of `grid`.
std::optional<std::string> musical_time(SamplePos pos, const MusicalContext& grid) {
  if (grid.bpm <= 0 || pos < grid.downbeat_offset) return std::nullopt;
  const double beats = static_cast<double>(pos - grid.downbeat_offset) * grid.bpm /
                       (60.0 * static_cast<double>(kSampleRate));
  for (int d = 0; d <= 3; ++d) {
    const long long units = std::llround(beats * kPow10[d]);
    const long long per_bar = grid.beats_per_bar * kPow10[d];
    const long long bar = units / per_bar + 1;
    const long long beat = units % per_bar + kPow10[d];  // 1-based
    std::string text = "bar " + std::to_string(bar);
    if (beat != kPow10[d]) text += " beat " + fixed(beat, d);
    if (parse_cue_time(text, grid) == pos) return text;
  }
  return std::nullopt;
}

std::string cue_time(SamplePos pos, const MusicalContext& grid) {
  if (pos > 0) {
    if (auto m = musical_time(pos, grid)) return *m;
  }
  return clock_time(pos);
}

// "8 bars", "1 bar", "16 beats" or "30s".
std::string duration(SamplePos len, const MusicalContext& grid) {
  if (grid.bpm > 0) {
    const double beats = static_cast<double>(len) * grid.bpm / (60.0 * static_cast<double>(kSampleRate));
    for (const bool bars : {true, false}) {
      const double amount = bars ? beats / grid.beats_per_bar : beats;
      for (int d = 0; d <= 3; ++d) {
        const long long units = std::llround(amount * kPow10[d]);
        if (units <= 0) continue;
        const bool one = units == kPow10[d];
        const std::string text =
            fixed(units, d) + (bars ? (one ? " bar" : " bars") : (one ? " beat" : " beats"));
        if (parse_duration(text, grid) == len) return text;
      }
    }
  }
  return seconds_duration(len);
}

// ---- layout ----------------------------------------------------------------------------------

// A run of `key = value` lines, optionally with the `=` signs aligned. As in the hand-written
// examples, header and track keys are aligned and cue keys are not, so adding a key to a cue
// does not re-indent its other lines in a diff.
class Block {
 public:
  explicit Block(bool align, std::string indent = {}) : align_(align), indent_(std::move(indent)) {}

  void add(std::string key, std::string value) { lines_.emplace_back(std::move(key), std::move(value)); }

  void write_to(std::string& out) const {
    std::size_t width = 0;
    if (align_) {
      for (const auto& [key, value] : lines_) width = std::max(width, key.size());
    }
    for (const auto& [key, value] : lines_) {
      out += indent_ + key + std::string(width > key.size() ? width - key.size() : 0, ' ') + " = " + value + "\n";
    }
  }

 private:
  bool align_;
  std::string indent_;
  std::vector<std::pair<std::string, std::string>> lines_;
};

std::string inline_table(const std::vector<std::pair<std::string, std::string>>& entries) {
  std::string out = "{ ";
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (i > 0) out += ", ";
    out += entries[i].first + " = " + entries[i].second;
  }
  return out + " }";
}

std::string string_array(const std::vector<std::string>& items) {
  std::string out = "[";
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i > 0) out += ", ";
    out += quote(items[i]);
  }
  return out + "]";
}

std::string preview_value(const Collection& c) {
  const Preview& p = *c.preview;
  std::vector<std::pair<std::string, std::string>> entries;
  if (c.type != CollectionType::Interactive) {
    entries.emplace_back("track", std::to_string(p.track));
    entries.emplace_back("start", quote(clock_time(p.start)));
    entries.emplace_back("length", quote(seconds_duration(p.length)));
  }
  if (!p.preset.empty() || c.type == CollectionType::Interactive) {
    entries.emplace_back("preset", quote(p.preset));
  }
  return inline_table(entries);
}

void write_track(std::string& out, const Track& t, CollectionType type) {
  out += "\n[[track]]\n";
  Block b(true);
  if (type == CollectionType::Album) b.add("file", quote(t.file));
  else b.add("from", quote(t.from));
  if (!t.title.empty()) b.add("title", quote(t.title));
  if (t.grid.bpm > 0) b.add("bpm", number(t.grid.bpm));
  if (t.grid.beats_per_bar != MusicalContext{}.beats_per_bar) {
    b.add("beats_per_bar", std::to_string(t.grid.beats_per_bar));
  }
  if (t.grid.downbeat_offset != 0) b.add("downbeat_offset", quote(clock_time(t.grid.downbeat_offset)));
  b.write_to(out);

  for (const Cue& cue : t.cues) {
    out += "\n  [[track.cue]]\n";
    Block cb(false, "  ");
    cb.add("at", quote(cue_time(cue.at, t.grid)));
    if (cue.pool) cb.add("pool", quote(pool_text(*cue.pool)));
    else cb.add("preset", quote(cue.preset));
    if (cue.every) cb.add("every", quote(duration(*cue.every, t.grid)));
    if (cue.transition) cb.add("transition", quote(transition_text(*cue.transition)));
    cb.write_to(out);
  }
}

}  // namespace

std::string write_manifest(const Collection& c) {
  std::string out;

  Block top(true);
  top.add("format", "1");
  top.add("type", quote(to_string(c.type)));
  top.add("title", quote(c.title));
  if (!c.artist.empty()) top.add("artist", quote(c.artist));
  if (!c.tags.empty()) top.add("tags", string_array(c.tags));
  if (!c.cover.empty()) top.add("cover", quote(c.cover));
  if (c.type == CollectionType::Interactive && c.preset_pool) {
    top.add("preset_pool", quote(pool_text(*c.preset_pool)));
  }
  if (c.preview) top.add("preview", preview_value(c));
  // TOML integers are signed 64-bit; the parser casts back, so large seeds survive as negatives.
  if (c.seed != default_seed(c.title)) top.add("seed", std::to_string(static_cast<std::int64_t>(c.seed)));
  top.write_to(out);

  out += "\n[defaults]\n";
  Block defaults(true);
  defaults.add("transition", quote(transition_text(c.default_transition)));
  defaults.write_to(out);

  if (c.type != CollectionType::Interactive) {
    for (const Track& t : c.tracks) write_track(out, t, c.type);
  }
  return out;
}

}  // namespace rm
