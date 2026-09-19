// rm-validate: check a collection directory against the collection.toml format.
// Exit codes: 0 = valid (warnings allowed), 1 = validation errors, 2 = usage or I/O problem.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "core/manifest.hpp"

namespace {

int usage() {
  std::cerr << "usage: rm-validate [--no-check-files] <collection-dir>\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  bool check_referenced_files = true;
  std::filesystem::path dir;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--no-check-files") {
      check_referenced_files = false;
    } else if (dir.empty() && !arg.starts_with("--")) {
      dir = arg;
    } else {
      return usage();
    }
  }
  if (dir.empty()) return usage();

  const std::filesystem::path manifest_path = dir / "collection.toml";
  std::ifstream in(manifest_path, std::ios::binary);
  if (!in) {
    std::cerr << "cannot read " << manifest_path.generic_string() << "\n";
    return 2;
  }
  std::ostringstream text;
  text << in.rdbuf();

  rm::ParseResult result = rm::parse_manifest(text.str());
  if (result.ok() && check_referenced_files) {
    rm::check_files(*result.collection, dir, result.diagnostics);
  }

  int errors = 0;
  int warnings = 0;
  for (const rm::Diagnostic& d : result.diagnostics) {
    const bool is_error = d.severity == rm::Severity::Error;
    (is_error ? errors : warnings)++;
    std::cerr << manifest_path.generic_string();
    if (d.line > 0) std::cerr << ":" << d.line;
    std::cerr << ": " << (is_error ? "error" : "warning") << ": ";
    if (!d.path.empty()) std::cerr << "[" << d.path << "] ";
    std::cerr << d.message << "\n";
  }

  if (errors == 0 && result.ok()) {
    std::cerr << manifest_path.generic_string() << ": OK (" << rm::to_string(result.collection->type)
              << ", " << result.collection->tracks.size() << " track(s), " << warnings
              << " warning(s))\n";
    return 0;
  }
  std::cerr << errors << " error(s), " << warnings << " warning(s)\n";
  return 1;
}
