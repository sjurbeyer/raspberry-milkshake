#include "ui/library_filter.hpp"

#include "ui/library_model.hpp"

namespace rm {

LibraryFilter::LibraryFilter(QObject* parent) : QSortFilterProxyModel(parent) {
  for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved}) {
    connect(this, signal, this, &LibraryFilter::countChanged);
  }
  connect(this, &QAbstractItemModel::modelReset, this, &LibraryFilter::countChanged);
  connect(this, &QAbstractItemModel::layoutChanged, this, &LibraryFilter::countChanged);
}

void LibraryFilter::setTypeFilter(const QString& type) {
  if (type == type_filter_) return;
  type_filter_ = type;
  invalidateFilter();
  emit typeFilterChanged();
  emit countChanged();
}

void LibraryFilter::setTagFilter(const QString& tag) {
  if (tag == tag_filter_) return;
  tag_filter_ = tag;
  invalidateFilter();
  emit tagFilterChanged();
  emit countChanged();
}

bool LibraryFilter::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
  const QAbstractItemModel* model = sourceModel();
  if (!model) return false;
  const QModelIndex row = model->index(source_row, 0, source_parent);
  if (type_filter_ != "all" && model->data(row, LibraryModel::TypeRole).toString() != type_filter_) return false;
  if (tag_filter_ != "all" && !model->data(row, LibraryModel::TagsRole).toStringList().contains(tag_filter_)) {
    return false;
  }
  return true;
}

QString LibraryFilter::idAt(int row) const { return data(index(row, 0), LibraryModel::IdRole).toString(); }
QString LibraryFilter::typeAt(int row) const { return data(index(row, 0), LibraryModel::TypeRole).toString(); }
QString LibraryFilter::titleAt(int row) const { return data(index(row, 0), LibraryModel::TitleRole).toString(); }

int LibraryFilter::rowOf(const QString& id) const {
  for (int row = 0; row < rowCount(); ++row) {
    if (idAt(row) == id) return row;
  }
  return -1;
}

}  // namespace rm
