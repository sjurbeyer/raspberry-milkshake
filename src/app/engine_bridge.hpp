#pragma once

#include <QObject>

namespace rm {

class AppController;
class Engine;
class LibraryModel;

// Connects the touch UI's controller to the engine: what the UI asks for (preview, play, stop)
// becomes engine commands, and when a collection finishes playing the controller moves on to the
// next queued one. Lives on the UI thread; the engine's render thread only ever sees commands.
class EngineBridge : public QObject {
  Q_OBJECT

 public:
  EngineBridge(AppController* controller, const LibraryModel* library, Engine* engine, QObject* parent = nullptr);
  ~EngineBridge() override;

 private:
  AppController* controller_;
  const LibraryModel* library_;
  Engine* engine_;
};

}  // namespace rm
