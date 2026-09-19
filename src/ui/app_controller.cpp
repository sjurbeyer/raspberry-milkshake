#include "ui/app_controller.hpp"

#include "ui/library_model.hpp"

namespace rm {

AppController::AppController(const LibraryModel* library, QObject* parent) : QObject(parent), library_(library) {}

QString AppController::nowPlayingId() const {
  return queue_.current() ? QString::fromStdString(*queue_.current()) : QString();
}

QString AppController::nowPlayingTitle() const { return library_->titleFor(nowPlayingId()); }

QStringList AppController::upcomingTitles() const {
  QStringList titles;
  for (const std::string& id : queue_.upcoming()) titles << library_->titleFor(QString::fromStdString(id));
  return titles;
}

QVariantMap AppController::browseActions(const QString& id) const {
  const auto type = library_->typeFor(id);
  BrowseActions actions;
  if (type) actions = browse_actions(queue_.state(), *type);
  return {{"previewAllowed", actions.preview_allowed},
          {"addToQueue", actions.add_to_queue},
          {"playNow", actions.play_now}};
}

void AppController::browsing(const QString& id) {
  if (!browseActions(id).value("previewAllowed").toBool()) {
    stop_preview();
    return;
  }
  if (preview_id_ == id) return;
  preview_id_ = id;
  emit previewRequested(id);
}

void AppController::stopBrowsing() { stop_preview(); }

void AppController::stop_preview() {
  if (preview_id_.isEmpty()) return;
  preview_id_.clear();
  emit previewStopped();
}

void AppController::playNow(const QString& id) {
  if (!library_->typeFor(id)) return;
  stop_preview();
  queue_.play_now(id.toStdString());
  emit playRequested(id);
  emit transportChanged();
  emit queueChanged();
}

void AppController::addToQueue(const QString& id) {
  if (!browseActions(id).value("addToQueue").toBool()) return;
  queue_.add(id.toStdString());
  emit queueChanged();
}

void AppController::pause() {
  if (queue_.state() != TransportState::Playing) return;
  queue_.pause();
  emit pauseRequested();
  emit transportChanged();
}

void AppController::resume() {
  if (queue_.state() != TransportState::Paused) return;
  queue_.resume();
  emit resumeRequested();
  emit transportChanged();
}

void AppController::togglePause() {
  if (paused()) resume();
  else pause();
}

void AppController::stopAndClear() {
  if (queue_.state() == TransportState::Idle) return;
  stop_preview();
  queue_.clear();
  emit stopped();
  emit transportChanged();
  emit queueChanged();
}

void AppController::next() {
  const auto now = queue_.next();
  if (now) emit playRequested(QString::fromStdString(*now));
  else emit stopped();
  emit transportChanged();
  emit queueChanged();
}

}  // namespace rm
