#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/sequencer.hpp"

namespace rm {

// A preset catalog backed by a directory of .milk files, scanned once at construction. Paths are
// relative to the root with forward slashes, sorted. `tag:` pools are empty until rm-bench provides
// tags.
//
// Ratings follow MilkDrop: each file's `fRating` (0 to 5, 3 if the file has none). A preset rated 0
// is never returned by resolve(), so it is never picked at random; presets named explicitly in a
// show are unaffected. set_rating() edits the file, and may be called from any thread. Copies of a
// catalog share their state.
class DirCatalog final : public IPresetCatalog {
 public:
  static constexpr double kDefaultRating = 3.0;

  explicit DirCatalog(const std::filesystem::path& root);

  std::vector<std::string> resolve(const PoolExpr& pool) const override;

  std::size_t size() const;          // every preset found, including hidden ones
  std::size_t hidden_count() const;  // presets rated 0
  std::optional<double> rating(const std::string& preset) const;  // nullopt for an unknown preset
  bool set_rating(const std::string& preset, double rating, std::string* error = nullptr);

 private:
  struct Entry {
    std::string name;
    double rating = kDefaultRating;
  };
  struct State {
    std::filesystem::path root;
    mutable std::mutex mutex;
    std::vector<Entry> entries;  // sorted by name
  };
  std::shared_ptr<State> state_;
};

}  // namespace rm
