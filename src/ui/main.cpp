// rm-touch-ui: the touch display's home screen. In the dev container it runs on Qt's VNC platform
// plugin (-platform vnc) and is shown in the browser dashboard; on the Pi it runs full screen.

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QImage>
#include <QTimer>
#include <QQuickItem>
#include <QQuickView>
#include <QtDebug>

#include "ui/app_controller.hpp"
#include "ui/library_filter.hpp"
#include "ui/library_model.hpp"
#include "ui/svg_image_provider.hpp"

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName("rm-touch-ui");

  QCommandLineParser parser;
  parser.addHelpOption();
  const QCommandLineOption library_option("library", "Folder holding the collections.", "dir",
                                          "examples/collections");
  parser.addOption(library_option);
  const QCommandLineOption screenshot_option("screenshot", "Save a screenshot of the window to <file> and exit.", "file");
  const QCommandLineOption view_option("view", "Start in the grid or swipe view.", "view");
  const QCommandLineOption select_option("select", "Open this collection id in the swipe view.", "id");
  const QCommandLineOption play_option("play", "Start playing this collection id.", "id");
  const QCommandLineOption pause_option("pause", "Pause what --play started.");
  const QCommandLineOption ask_stop_option("ask-stop", "Open the stop confirmation.");
  parser.addOptions({screenshot_option, view_option, select_option, play_option, pause_option, ask_stop_option});
  parser.process(app);

  rm::LibraryModel model;
  model.load(parser.value(library_option));
  for (const QString& problem : model.problems()) qWarning().noquote() << "skipped collection:" << problem;
  qInfo().noquote() << model.rowCount() << "collections from" << parser.value(library_option);

  rm::LibraryFilter filter;
  filter.setSourceModel(&model);
  rm::AppController controller(&model);

  // Until the engine is connected, log what it would be asked to do.
  QObject::connect(&controller, &rm::AppController::previewRequested,
                   [](const QString& id) { qInfo().noquote() << "preview:" << id; });
  QObject::connect(&controller, &rm::AppController::previewStopped, [] { qInfo() << "preview stopped"; });
  QObject::connect(&controller, &rm::AppController::playRequested,
                   [](const QString& id) { qInfo().noquote() << "play:" << id; });
  QObject::connect(&controller, &rm::AppController::stopped, [] { qInfo() << "stopped"; });

  QQuickView view;
  view.setTitle("raspberry milkshake");
  view.setResizeMode(QQuickView::SizeRootObjectToView);
  view.engine()->addImageProvider("svg", new rm::SvgImageProvider);
  view.setInitialProperties({{"libraryFilter", QVariant::fromValue<QObject*>(&filter)},
                             {"controller", QVariant::fromValue<QObject*>(&controller)},
                             {"tags", model.allTags()}});
  view.loadFromModule("Milkshake", "HomeScreen");
  if (view.status() != QQuickView::Ready) {
    qCritical() << "could not load the home screen";
    return 1;
  }
  view.resize(1280, 720);
  view.show();

  QQuickItem* root = view.rootObject();
  if (parser.isSet(play_option)) controller.playNow(parser.value(play_option));
  if (parser.isSet(pause_option)) controller.pause();
  if (parser.isSet(view_option)) root->setProperty("view", parser.value(view_option));
  if (parser.isSet(select_option)) {
    QMetaObject::invokeMethod(root, "openSwipe", Q_ARG(QVariant, filter.rowOf(parser.value(select_option))));
  }
  if (parser.isSet(ask_stop_option)) {
    if (QObject* dialog = root->findChild<QObject*>("stopDialog")) QMetaObject::invokeMethod(dialog, "open");
  }
  if (parser.isSet(screenshot_option)) {
    const QString file = parser.value(screenshot_option);
    QTimer::singleShot(1500, [&view, &app, file] {
      const bool saved = view.grabWindow().save(file);
      qInfo().noquote() << (saved ? "saved" : "could not save") << file;
      app.exit(saved ? 0 : 1);
    });
  }
  return app.exec();
}
