#pragma once

#include <QSortFilterProxyModel>

namespace rm {

// The rows of a LibraryModel that match the type and tag dropdowns. "all" disables a filter.
class LibraryFilter : public QSortFilterProxyModel {
  Q_OBJECT
  Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY typeFilterChanged)
  Q_PROPERTY(QString tagFilter READ tagFilter WRITE setTagFilter NOTIFY tagFilterChanged)
  Q_PROPERTY(int count READ count NOTIFY countChanged)

 public:
  explicit LibraryFilter(QObject* parent = nullptr);

  int count() const { return rowCount(); }
  QString typeFilter() const { return type_filter_; }
  QString tagFilter() const { return tag_filter_; }
  void setTypeFilter(const QString& type);
  void setTagFilter(const QString& tag);

  Q_INVOKABLE QString idAt(int row) const;
  Q_INVOKABLE QString typeAt(int row) const;
  Q_INVOKABLE QString titleAt(int row) const;
  // Row of the collection in the filtered list, or -1 if it is filtered out.
  Q_INVOKABLE int rowOf(const QString& id) const;

 signals:
  void typeFilterChanged();
  void tagFilterChanged();
  void countChanged();

 protected:
  bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

 private:
  QString type_filter_ = "all";
  QString tag_filter_ = "all";
};

}  // namespace rm
