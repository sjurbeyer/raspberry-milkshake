#include "core/cue_time.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <optional>
#include <vector>

namespace rm {
namespace {

std::string normalize(std::string_view s) {
  std::string out;
  for (char c : s) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  const auto first = out.find_first_not_of(" \t");
  if (first == std::string::npos) return {};
  const auto last = out.find_last_not_of(" \t");
  return out.substr(first, last - first + 1);
}

std::vector<std::string_view> split_whitespace(std::string_view s) {
  std::vector<std::string_view> parts;
  std::size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    const std::size_t start = i;
    while (i < s.size() && s[i] != ' ' && s[i] != '\t') ++i;
    if (i > start) parts.push_back(s.substr(start, i - start));
  }
  return parts;
}

std::optional<double> to_double(std::string_view s) {
  double v = 0;
  const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
  if (ec != std::errc{} || ptr != s.data() + s.size() || !std::isfinite(v)) return std::nullopt;
  return v;
}

std::optional<long long> to_int(std::string_view s) {
  long long v = 0;
  const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
  if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
  return v;
}

SamplePos seconds_to_samples(double seconds) {
  return static_cast<SamplePos>(std::llround(seconds * static_cast<double>(kSampleRate)));
}

SamplePos beats_to_samples(double beats, double bpm) {
  return seconds_to_samples(beats * 60.0 / bpm);
}

std::unexpected<std::string> fail(std::string msg) { return std::unexpected(std::move(msg)); }

TimeResult parse_clock(std::string_view s) {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == ':') {
      parts.push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  if (parts.size() < 2 || parts.size() > 3) return fail("expected M:SS or H:MM:SS");

  const auto seconds = to_double(parts.back());
  if (!seconds || *seconds < 0 || *seconds >= 60) return fail("seconds must be in [0, 60)");

  long long minutes = 0;
  long long hours = 0;
  if (parts.size() == 2) {
    const auto m = to_int(parts[0]);
    if (!m || *m < 0) return fail("invalid minutes");
    minutes = *m;
  } else {
    const auto h = to_int(parts[0]);
    const auto m = to_int(parts[1]);
    if (!h || *h < 0) return fail("invalid hours");
    if (!m || *m < 0 || *m >= 60) return fail("minutes must be in [0, 60)");
    hours = *h;
    minutes = *m;
  }
  return seconds_to_samples(static_cast<double>(hours * 3600 + minutes * 60) + *seconds);
}

}  // namespace

TimeResult parse_cue_time(std::string_view text, const MusicalContext& ctx) {
  const std::string s = normalize(text);
  if (s.empty()) return fail("empty time");

  if (s.starts_with("bar")) {
    const auto tokens = split_whitespace(s);
    const bool with_beat = tokens.size() == 4;
    if (!(tokens.size() == 2 || with_beat) || tokens[0] != "bar" ||
        (with_beat && tokens[2] != "beat")) {
      return fail("expected \"bar N\" or \"bar N beat B\"");
    }
    if (ctx.bpm <= 0) return fail("musical time needs a bpm on the track");
    const auto bar = to_int(tokens[1]);
    if (!bar || *bar < 1) return fail("bar must be an integer >= 1");
    double beat = 1.0;
    if (with_beat) {
      const auto b = to_double(tokens[3]);
      if (!b || *b < 1.0 || *b >= ctx.beats_per_bar + 1) {
        return fail("beat must be in [1, beats_per_bar + 1)");
      }
      beat = *b;
    }
    const double beats = static_cast<double>((*bar - 1) * ctx.beats_per_bar) + (beat - 1.0);
    return ctx.downbeat_offset + beats_to_samples(beats, ctx.bpm);
  }

  if (s.find(':') != std::string::npos) return parse_clock(s);

  if (s.back() == 's') {
    const auto seconds = to_double(std::string_view(s).substr(0, s.size() - 1));
    if (!seconds || *seconds < 0) return fail("invalid seconds value");
    return seconds_to_samples(*seconds);
  }
  return fail("unrecognized time (use M:SS, H:MM:SS, <n>s, or bar N [beat B])");
}

TimeResult parse_duration(std::string_view text, const MusicalContext& ctx) {
  const std::string s = normalize(text);
  if (s.empty()) return fail("empty duration");

  SamplePos samples = 0;
  const auto tokens = split_whitespace(s);
  if (tokens.size() == 1 && tokens[0].back() == 's') {
    const auto seconds = to_double(tokens[0].substr(0, tokens[0].size() - 1));
    if (!seconds || *seconds <= 0) return fail("duration must be positive");
    samples = seconds_to_samples(*seconds);
  } else if (tokens.size() == 2) {
    const auto amount = to_double(tokens[0]);
    if (!amount || *amount <= 0) return fail("duration must be positive");
    const bool bars = tokens[1] == "bar" || tokens[1] == "bars";
    const bool beats = tokens[1] == "beat" || tokens[1] == "beats";
    if (!bars && !beats) return fail("unit must be bars, beats or s");
    if (ctx.bpm <= 0) return fail("musical duration needs a bpm on the track");
    samples = beats_to_samples(bars ? *amount * ctx.beats_per_bar : *amount, ctx.bpm);
  } else {
    return fail("expected \"<n> bars\", \"<n> beats\" or \"<n>s\"");
  }
  if (samples < 1) return fail("duration is shorter than one sample");
  return samples;
}

}  // namespace rm
