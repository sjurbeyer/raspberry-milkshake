#include "ui/library_model.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <set>

namespace rm {

LibraryModel::LibraryModel(QObject* parent) : QAbstractListModel(parent) {}

void LibraryModel::load(const QString& root) {
  beginResetModel();
  entries_.clear();
  problems_.clear();

  const QDir dir(root);
  for (const QFileInfo& sub : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
    QFile file(sub.absoluteFilePath() + "/collection.toml");
    if (!file.open(QIODevice::ReadOnly)) continue;  // not a collection folder
    const QByteArray bytes = file.readAll();
    const ParseResult result = parse_manifest(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
    if (!result.ok()) {
      QString reason = "invalid collection.toml";
      for (const Diagnostic& d : result.diagnostics) {
        if (d.severity == Severity::Error) {
          reason = QString::fromStdString(d.path.empty() ? d.message : d.path + ": " + d.message);
          break;
        }
      }
      problems_ << sub.fileName() + ": " + reason;
      continue;
    }

    const Collection& c = *result.collection;
    Entry e;
    e.id = sub.fileName();
    e.title = QString::fromStdString(c.title);
    e.artist = QString::fromStdString(c.artist);
    e.type = QString::fromUtf8(to_string(c.type).data(), static_cast<qsizetype>(to_string(c.type).size()));
    e.type_enum = c.type;
    for (const std::string& tag : c.tags) e.tags << QString::fromStdString(tag);
    e.track_count = static_cast<int>(c.tracks.size());
    e.collection = c;
    if (!c.cover.empty()) {
      const QString cover_path = sub.absoluteFilePath() + "/" + QString::fromStdString(c.cover);
      if (QFileInfo::exists(cover_path)) e.cover = QUrl::fromLocalFile(cover_path);
    }
    entries_.push_back(std::move(e));
  }

  std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
    const bool a_interactive = a.type_enum == CollectionType::Interactive;
    const bool b_interactive = b.type_enum == CollectionType::Interactive;
    if (a_interactive != b_interactive) return a_interactive;
    return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
  });

  std::set<QString> tags;
  for (const Entry& e : entries_) tags.insert(e.tags.begin(), e.tags.end());
  all_tags_ = QStringList(tags.begin(), tags.end());

  endResetModel();
  emit loaded();
}

int LibraryModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) return {};
  const Entry& e = entries_[static_cast<std::size_t>(index.row())];
  switch (role) {
    case IdRole: return e.id;
    case TitleRole: return e.title;
    case ArtistRole: return e.artist;
    case TypeRole: return e.type;
    case TagsRole: return e.tags;
    case CoverRole: return e.cover;
    case TrackCountRole: return e.track_count;
    default: return {};
  }
}

QHash<int, QByteArray> LibraryModel::roleNames() const {
  return {{IdRole, "collectionId"}, {TitleRole, "title"},   {ArtistRole, "artist"},
          {TypeRole, "collectionType"}, {TagsRole, "tags"}, {CoverRole, "cover"},
          {TrackCountRole, "trackCount"}};
}

const LibraryModel::Entry* LibraryModel::find(const QString& id) const {
  const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.id == id; });
  return it == entries_.end() ? nullptr : &*it;
}

QString LibraryModel::titleFor(const QString& id) const {
  const Entry* e = find(id);
  return e ? e->title : QString();
}

const Collection* LibraryModel::collection(const QString& id) const {
  const Entry* e = find(id);
  return e ? &e->collection : nullptr;
}

std::optional<CollectionType> LibraryModel::typeFor(const QString& id) const {
  const Entry* e = find(id);
  if (!e) return std::nullopt;
  return e->type_enum;
}

}  // namespace rm
