#include "ui/svg_image_provider.hpp"

#include <QPainter>
#include <QSvgRenderer>
#include <QUrl>

namespace rm {

SvgImageProvider::SvgImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage SvgImageProvider::requestImage(const QString& id, QSize* size, const QSize& requested_size) {
  const QString url = id.contains('%') ? QUrl::fromPercentEncoding(id.toUtf8()) : id;
  QString path;
  if (url.startsWith("qrc:")) path = ":" + url.mid(4);  // qrc:/qt/qml/... -> :/qt/qml/...
  else if (url.startsWith("file:")) path = QUrl(url).toLocalFile();
  else path = url;

  QSvgRenderer renderer(path);
  if (!renderer.isValid()) return {};

  const QSize natural = renderer.defaultSize();
  if (size) *size = natural;
  const QSize target = requested_size.isValid() && !requested_size.isEmpty() ? requested_size : natural;
  QImage image(target, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  renderer.render(&painter);
  return image;
}

}  // namespace rm
