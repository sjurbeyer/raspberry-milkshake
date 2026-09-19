#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include "core/policy.hpp"

namespace rm {

class LibraryModel;

// What the touch UI can ask for. It holds the play queue and applies the browse rules from
// core/policy; it does not play anything itself: the engine listens to the signals.
class AppController : public QObject {
  Q_OBJECT
  // `playing` means there is a current item, whether it is running or paused.
  Q_PROPERTY(bool playing READ playing NOTIFY transportChanged)
  Q_PROPERTY(bool paused READ paused NOTIFY transportChanged)
  Q_PROPERTY(QString nowPlayingId READ nowPlayingId NOTIFY transportChanged)
  Q_PROPERTY(QString nowPlayingTitle READ nowPlayingTitle NOTIFY transportChanged)
  Q_PROPERTY(QStringList upcomingTitles READ upcomingTitles NOTIFY queueChanged)
  Q_PROPERTY(int queueLength READ queueLength NOTIFY queueChanged)

 public:
  explicit AppController(const LibraryModel* library, QObject* parent = nullptr);

  bool playing() const { return queue_.state() != TransportState::Idle; }
  bool paused() const { return queue_.state() == TransportState::Paused; }
  QString nowPlayingId() const;
  QString nowPlayingTitle() const;
  QStringList upcomingTitles() const;
  int queueLength() const { return static_cast<int>(queue_.upcoming().size()); }

  // {previewAllowed, addToQueue, playNow} for the collection, per the current transport state.
  Q_INVOKABLE QVariantMap browseActions(const QString& id) const;

  // The swipe view is showing this collection: asks for a preview if one is allowed right now,
  // otherwise stops any running preview.
  Q_INVOKABLE void browsing(const QString& id);
  Q_INVOKABLE void stopBrowsing();

  Q_INVOKABLE void playNow(const QString& id);
  Q_INVOKABLE void addToQueue(const QString& id);
  Q_INVOKABLE void next();
  // Pause and continue the current item; the queue is untouched. Next or play now end the pause.
  Q_INVOKABLE void pause();
  Q_INVOKABLE void resume();
  Q_INVOKABLE void togglePause();
  // Ends the current item and empties the queue. The UI asks the user first.
  Q_INVOKABLE void stopAndClear();

 signals:
  void transportChanged();
  void queueChanged();
  void previewRequested(const QString& id);
  void previewStopped();
  void playRequested(const QString& id);
  void pauseRequested();
  void resumeRequested();
  void stopped();

 private:
  void stop_preview();

  const LibraryModel* library_;
  Queue queue_;
  QString preview_id_;
};

}  // namespace rm
