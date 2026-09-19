#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QUrl>
#include <optional>
#include <vector>

#include "core/manifest.hpp"

namespace rm {

// The collections found under a library folder, one row per valid collection. Interactive
// collections come first, then everything else alphabetically. Folders with an invalid
// collection.toml are skipped and listed in `problems`.
class LibraryModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(QStringList allTags READ allTags NOTIFY loaded)
  Q_PROPERTY(QStringList problems READ problems NOTIFY loaded)

 public:
  // "id" is reserved in QML, so the role names are prefixed.
  enum Role {
    IdRole = Qt::UserRole + 1,
    TitleRole,
    ArtistRole,
    TypeRole,
    TagsRole,
    CoverRole,
    TrackCountRole,
  };

  explicit LibraryModel(QObject* parent = nullptr);

  Q_INVOKABLE void load(const QString& root);

  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  const QStringList& allTags() const { return all_tags_; }
  const QStringList& problems() const { return problems_; }

  Q_INVOKABLE QString titleFor(const QString& id) const;
  std::optional<CollectionType> typeFor(const QString& id) const;
  // The parsed manifest, or null for an unknown id. Valid until the next load().
  const Collection* collection(const QString& id) const;

 signals:
  void loaded();

 private:
  struct Entry {
    QString id;
    QString title;
    QString artist;
    QString type;  // "album", "playlist" or "interactive"
    CollectionType type_enum = CollectionType::Album;
    QStringList tags;
    QUrl cover;  // empty if the collection has no cover file
    int track_count = 0;
    Collection collection;
  };
  const Entry* find(const QString& id) const;

  std::vector<Entry> entries_;
  QStringList all_tags_;
  QStringList problems_;
};

}  // namespace rm
