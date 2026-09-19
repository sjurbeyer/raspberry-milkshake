#include "core/dir_catalog.hpp"

#include <algorithm>
#include <system_error>

#include "core/preset_rating.hpp"

namespace rm {

DirCatalog::DirCatalog(const std::filesystem::path& root) : state_(std::make_shared<State>()) {
  namespace fs = std::filesystem;
  state_->root = root;
  std::error_code ec;
  if (!fs::is_directory(root, ec)) return;
  for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
       !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file(ec) && it->path().extension() == ".milk") {
      Entry entry;
      entry.name = fs::relative(it->path(), root, ec).generic_string();
      entry.rating = read_rating(it->path()).value_or(kDefaultRating);
      state_->entries.push_back(std::move(entry));
    }
  }
  std::sort(state_->entries.begin(), state_->entries.end(),
            [](const Entry& a, const Entry& b) { return a.name < b.name; });
}

std::vector<std::string> DirCatalog::resolve(const PoolExpr& pool) const {
  std::string prefix;
  if (pool.kind == PoolExpr::Kind::Tag) return {};
  if (pool.kind == PoolExpr::Kind::Dir) {
    prefix = pool.arg;
    while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();
    prefix += '/';
  }

  std::lock_guard lock(state_->mutex);
  std::vector<std::string> out;
  for (const Entry& e : state_->entries) {
    if (e.rating <= 0.0) continue;  // rated 0: never picked at random
    if (pool.kind == PoolExpr::Kind::Dir && !e.name.starts_with(prefix)) continue;
    out.push_back(e.name);
  }
  return out;
}

std::size_t DirCatalog::size() const {
  std::lock_guard lock(state_->mutex);
  return state_->entries.size();
}

std::size_t DirCatalog::hidden_count() const {
  std::lock_guard lock(state_->mutex);
  return static_cast<std::size_t>(
      std::count_if(state_->entries.begin(), state_->entries.end(), [](const Entry& e) { return e.rating <= 0.0; }));
}

std::optional<double> DirCatalog::rating(const std::string& preset) const {
  std::lock_guard lock(state_->mutex);
  const auto it = std::find_if(state_->entries.begin(), state_->entries.end(),
                               [&](const Entry& e) { return e.name == preset; });
  if (it == state_->entries.end()) return std::nullopt;
  return it->rating;
}

bool DirCatalog::set_rating(const std::string& preset, double rating, std::string* error) {
  std::lock_guard lock(state_->mutex);
  // Only names the catalog itself found can be written, so a request cannot reach outside the root.
  const auto it = std::find_if(state_->entries.begin(), state_->entries.end(),
                               [&](const Entry& e) { return e.name == preset; });
  if (it == state_->entries.end()) {
    if (error) *error = "unknown preset";
    return false;
  }
  if (!write_rating(state_->root / preset, rating, error)) return false;
  it->rating = rating;
  return true;
}

}  // namespace rm
