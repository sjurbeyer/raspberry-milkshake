#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

#include "ui/app_controller.hpp"
#include "ui/library_filter.hpp"
#include "ui/library_model.hpp"
#include "ui/svg_image_provider.hpp"

// Gives each QML test a fresh filter and controller over the example library.
class Harness : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList tags READ tags CONSTANT)

 public:
  explicit Harness(QObject* parent = nullptr) : QObject(parent) {
    model_.load(QStringLiteral(RM_SOURCE_DIR "/examples/collections"));
  }
  QStringList tags() const { return model_.allTags(); }
  Q_INVOKABLE QObject* newFilter() {
    auto* filter = new rm::LibraryFilter(this);
    filter->setSourceModel(&model_);
    return filter;
  }
  Q_INVOKABLE QObject* newController() { return new rm::AppController(&model_, this); }

 private:
  rm::LibraryModel model_;
};

class Setup : public QObject {
  Q_OBJECT

 public slots:
  void qmlEngineAvailable(QQmlEngine* engine) {
    engine->addImageProvider("svg", new rm::SvgImageProvider);
    engine->rootContext()->setContextProperty("harness", &harness_);
  }

 private:
  Harness harness_;
};

QUICK_TEST_MAIN_WITH_SETUP(rm_ui, Setup)

#include "tst_main.moc"
