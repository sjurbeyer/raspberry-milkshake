#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/sequencer.hpp"

namespace rm {

// A preset catalog backed by a directory of .milk files, scanned once at construction. Paths are
// relative to the root with forward slashes, sorted. `tag:` pools are empty until rm-bench provides
// tags.
class DirCatalog final : public IPresetCatalog {
 public:
  explicit DirCatalog(const std::filesystem::path& root);

  std::vector<std::string> resolve(const PoolExpr& pool) const override;
  std::size_t size() const { return presets_.size(); }

 private:
  std::vector<std::string> presets_;
};

}  // namespace rm
