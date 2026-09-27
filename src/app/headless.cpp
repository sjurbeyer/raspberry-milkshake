#include "app/headless.hpp"

#include "ui/library_model.hpp"

namespace rm {

bool should_run_headless(int screen_count, bool force_headless, bool force_ui) {
  if (force_headless) return true;
  if (force_ui) return false;
  return screen_count == 0;
}

QString find_default_interactive_id(const LibraryModel& model) {
  for (int row = 0; row < model.rowCount(); ++row) {
    const QModelIndex index = model.index(row, 0);
    if (model.data(index, LibraryModel::TypeRole).toString() == "interactive") {
      return model.data(index, LibraryModel::IdRole).toString();
    }
  }
  return {};
}

}  // namespace rm
