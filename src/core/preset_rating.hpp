#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace rm {

// MilkDrop stores a preset's rating (0 to 5) as a line `fRating=<number>` in the file. A rating of 0
// means "never pick this preset at random". These functions read and edit that line and leave every
// other byte of the file as it was.

// The rating in the given text (the first `fRating=` line), if there is one.
std::optional<double> parse_rating(std::string_view text);

// Reads the start of the file and returns its rating; nullopt if the file has none or cannot be read.
std::optional<double> read_rating(const std::filesystem::path& file);

// Sets the rating in place (or adds the line under `[preset00]` if the file has none). The file is
// replaced atomically. Returns false and fills `error` if the rating is outside 0..5, the file cannot
// be read or written, or it is not a MilkDrop preset.
bool write_rating(const std::filesystem::path& file, double rating, std::string* error = nullptr);

}  // namespace rm
