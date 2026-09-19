#include "core/preset_rating.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>

namespace rm {
namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
  return s;
}

struct RatingLine {
  std::size_t value_begin = 0;  // offsets into the text
  std::size_t value_end = 0;
  std::optional<double> value;
};

std::optional<RatingLine> find_rating_line(std::string_view text) {
  std::size_t pos = 0;
  while (pos <= text.size()) {
    std::size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view line = text.substr(pos, end - pos);
    const auto eq = line.find('=');
    if (eq != std::string_view::npos && trim(line.substr(0, eq)) == "fRating") {
      const std::string_view raw = line.substr(eq + 1);
      const std::string_view value = trim(raw);
      RatingLine r;
      r.value_begin = static_cast<std::size_t>(value.data() - text.data());
      r.value_end = r.value_begin + value.size();
      double v = 0;
      const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
      if (ec == std::errc{} && ptr == value.data() + value.size() && std::isfinite(v)) r.value = v;
      return r;
    }
    pos = end + 1;
  }
  return std::nullopt;
}

bool fail(std::string* error, std::string message) {
  if (error) *error = std::move(message);
  return false;
}

}  // namespace

std::optional<double> parse_rating(std::string_view text) {
  const auto line = find_rating_line(text);
  return line ? line->value : std::nullopt;
}

std::optional<double> read_rating(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return std::nullopt;
  std::string head(16384, '\0');
  in.read(head.data(), static_cast<std::streamsize>(head.size()));
  head.resize(static_cast<std::size_t>(in.gcount()));
  return parse_rating(head);
}

bool write_rating(const std::filesystem::path& file, double rating, std::string* error) {
  if (!(rating >= 0.0 && rating <= 5.0)) return fail(error, "rating must be between 0 and 5");

  std::string content;
  {
    std::ifstream in(file, std::ios::binary);
    if (!in) return fail(error, "cannot read " + file.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    content = ss.str();
  }

  char formatted[32];
  std::snprintf(formatted, sizeof formatted, "%.6f", rating);

  if (const auto line = find_rating_line(content)) {
    content.replace(line->value_begin, line->value_end - line->value_begin, formatted);
  } else {
    // No rating yet: add one under [preset00], using that line's own line ending.
    const auto marker = content.find("[preset00]");
    if (marker == std::string::npos) return fail(error, "not a MilkDrop preset (no [preset00] section)");
    std::size_t eol = content.find('\n', marker);
    std::string ending = "\n";
    if (eol == std::string::npos) {
      content += "\n";
      eol = content.size() - 1;
    } else if (eol > 0 && content[eol - 1] == '\r') {
      ending = "\r\n";
    }
    content.insert(eol + 1, std::string("fRating=") + formatted + ending);
  }

  const std::filesystem::path temp = file.string() + ".rm-tmp";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) return fail(error, "cannot write " + temp.string());
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) return fail(error, "write failed for " + temp.string());
  }
  std::error_code ec;
  std::filesystem::rename(temp, file, ec);
  if (ec) {
    std::filesystem::remove(temp, ec);
    return fail(error, "cannot replace " + file.string());
  }
  return true;
}

}  // namespace rm
