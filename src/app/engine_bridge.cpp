#include "app/engine_bridge.hpp"

#include <QMetaObject>

#include "render/engine.hpp"
#include "ui/app_controller.hpp"
#include "ui/library_model.hpp"

namespace rm {

EngineBridge::EngineBridge(AppController* controller, const LibraryModel* library, Engine* engine, QObject* parent)
    : QObject(parent), controller_(controller), library_(library), engine_(engine) {
  connect(controller, &AppController::previewRequested, this, [this](const QString& id) {
    if (const Collection* c = library_->collection(id)) engine_->preview(id.toStdString(), *c);
  });
  connect(controller, &AppController::previewStopped, this, [this] { engine_->stop_preview(); });
  connect(controller, &AppController::playRequested, this, [this](const QString& id) {
    if (const Collection* c = library_->collection(id)) engine_->play(id.toStdString(), *c);
  });
  connect(controller, &AppController::stopped, this, [this] { engine_->stop(); });

  // Runs on the render thread; hop to the UI thread before touching the controller.
  engine_->set_finished_callback(
      [controller] { QMetaObject::invokeMethod(controller, &AppController::next, Qt::QueuedConnection); });
}

EngineBridge::~EngineBridge() { engine_->set_finished_callback({}); }

}  // namespace rm
