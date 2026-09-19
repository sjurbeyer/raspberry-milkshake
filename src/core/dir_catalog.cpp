#include "core/dir_catalog.hpp"

#include <algorithm>
#include <system_error>

namespace rm {

DirCatalog::DirCatalog(const std::filesystem::path& root) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::is_directory(root, ec)) return;
  for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
       !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file(ec) && it->path().extension() == ".milk") {
      presets_.push_back(fs::relative(it->path(), root, ec).generic_string());
    }
  }
  std::sort(presets_.begin(), presets_.end());
}

std::vector<std::string> DirCatalog::resolve(const PoolExpr& pool) const {
  switch (pool.kind) {
    case PoolExpr::Kind::All:
      return presets_;
    case PoolExpr::Kind::Tag:
      return {};
    case PoolExpr::Kind::Dir: {
      std::string prefix = pool.arg;
      while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();
      prefix += '/';
      std::vector<std::string> out;
      for (const std::string& p : presets_) {
        if (p.starts_with(prefix)) out.push_back(p);
      }
      return out;
    }
  }
  return {};
}

}  // namespace rm
